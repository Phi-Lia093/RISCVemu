/*
 * SPDX-FileCopyrightText: 2026 PhiLia093 phi_lia093@126.com
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * This file is part of RISCVemu.
 * RISCVemu is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * RISCVemu is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include <device/uart.h>

#include <stdio.h>
#include <string.h>

#ifdef CONFIG_ENABLE_UART_DEVICE

#ifdef CONFIG_ENABLE_PLIC_DEVICE
#include <device/plic.h>
#endif

/* 16550 register defines (byte offsets). */
#define UART_THR 0u /* DLAB=0: transmit holding register */
#define UART_RBR 0u /* DLAB=0: receiver buffer register  */
#define UART_DLL 0u /* DLAB=1: divisor latch (low)       */
#define UART_IER 1u
#define UART_DLH 1u /* DLAB=1: divisor latch (high)      */
#define UART_IIR 2u
#define UART_FCR 2u
#define UART_LCR 3u
#define UART_MCR 4u
#define UART_LSR 5u
#define UART_MSR 6u
#define UART_SCR 7u

/* IER bits. */
#define IER_ERBI (1u << 0) /* enable received-data-available interrupt */

/* LSR bits. */
#define LSR_DR (1u << 0)   /* data ready */
#define LSR_THRE (1u << 5) /* transmit holding register empty */
#define LSR_TEMT (1u << 6) /* transmitter empty */

/* IIR values (no FIFO state modelled, so only RX-pending matters). */
#define IIR_NOPEND 0x01
#define IIR_RXDATA 0x04 /* received data available */

/* LCR DLAB bit. */
#define LCR_DLAB (1u << 7)

#define RX_FIFO_SIZE 256u

static struct
{
    uint8_t rx[RX_FIFO_SIZE]; /* RX ring buffer */
    unsigned rx_head;         /* next read position */
    unsigned rx_count;

    uint8_t ier; /* interrupt-enable register */
    uint8_t lcr; /* line-control register */
    uint8_t mcr;
    uint8_t scr;

    int irq_level; /* whether PLIC line is currently asserted */
} u;

static void
uart_update_irq(void)
{
    int want = 0;
#ifdef CONFIG_ENABLE_PLIC_DEVICE
    if ((u.rx_count > 0) && (u.ier & IER_ERBI)) want = 1;
    if (want != u.irq_level)
    {
        u.irq_level = want;
        plic_set_irq(UART_IRQ, want != 0);
    }
#else
    (void)want;
    u.irq_level = 0;
#endif
}

void
uart_init(void)
{
    memset(&u, 0, sizeof(u));
    u.lcr = 0x03; /* 8N1 */
    u.irq_level = 0;
    uart_update_irq();
}

void
uart_receive(uint8_t c)
{
    if (u.rx_count >= RX_FIFO_SIZE) return; /* FIFO full; drop. */
    unsigned pos = (u.rx_head + u.rx_count) % RX_FIFO_SIZE;
    u.rx[pos] = c;
    u.rx_count++;
    uart_update_irq();
}

uint32_t
uart_read(uint32_t offset)
{
    switch (offset & 0x7u)
    {
    case UART_RBR:
        if (u.lcr & LCR_DLAB) return 0; /* DLL not implemented */
        if (u.rx_count > 0)
        {
            uint32_t b = u.rx[u.rx_head];
            u.rx_head = (u.rx_head + 1) % RX_FIFO_SIZE;
            u.rx_count--;
            uart_update_irq();
            return b;
        }
        return 0;
    case UART_IER:
        return u.lcr & LCR_DLAB ? 0 /* DLH not implemented */ : u.ier;
    case UART_IIR:
        if (u.rx_count > 0 && (u.ier & IER_ERBI)) return IIR_RXDATA;
        return IIR_NOPEND;
    case UART_LCR:
        return u.lcr;
    case UART_MCR:
        return u.mcr;
    case UART_LSR:
        return LSR_THRE | LSR_TEMT | (u.rx_count > 0 ? LSR_DR : 0);
    case UART_MSR:
        return 0;
    default:
        return u.scr;
    }
}

void
uart_write(uint32_t offset, uint32_t val)
{
    switch (offset & 0x7u)
    {
    case UART_THR:
        if (u.lcr & LCR_DLAB) break; /* DLL not implemented */
        putc((int)(val & 0xFFu), stdout);
        fflush(stdout);
        break;
    case UART_IER:
        u.ier = (uint8_t)(val & 0x0Fu);
        uart_update_irq();
        break;
    case UART_FCR:
        uart_update_irq();
        break;
    case UART_LCR:
        u.lcr = (uint8_t)val;
        break;
    case UART_MCR:
        u.mcr = (uint8_t)val;
        break;
    default:
        u.scr = (uint8_t)val;
        break;
    }
}

/* ------------------------------------------------------------------ */
/* Async host input: epoll (ready) / poll (fallback), single-threaded  */
/* ------------------------------------------------------------------ */

#ifdef CONFIG_ENABLE_UART_INPUT

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#if defined(__linux__)
#include <sys/epoll.h>
#define UART_INPUT_EPOLL 1
#else
#include <poll.h>
#endif

static int uart_input_active = 0;

void
uart_input_setup(void)
{
    /* Non-blocking stdin so the loop never stalls on a host read. */
    int fl = fcntl(STDIN_FILENO, F_GETFL, 0);
    if (fl < 0) return;
    fcntl(STDIN_FILENO, F_SETFL, fl | O_NONBLOCK);
    uart_input_active = 1;
}

void
uart_poll_input(void)
{
    if (!uart_input_active) return;

#if UART_INPUT_EPOLL
    static int epfd = -1;
    if (epfd < 0)
    {
        epfd = epoll_create1(0);
        if (epfd < 0)
        {
            uart_input_active = 0;
            return;
        }
        struct epoll_event ev;
        memset(&ev, 0, sizeof(ev));
        ev.events = EPOLLIN;
        ev.data.fd = STDIN_FILENO;
        if (epoll_ctl(epfd, EPOLL_CTL_ADD, STDIN_FILENO, &ev) < 0)
        {
            uart_input_active = 0;
            return;
        }
    }

    struct epoll_event ev;
    int n = epoll_wait(epfd, &ev, 1, 0); /* timeout 0: never block the loop */
    if (n <= 0) return;
#else
    struct pollfd pfd;
    pfd.fd = STDIN_FILENO;
    pfd.events = POLLIN;
    pfd.revents = 0;
    if (poll(&pfd, 1, 0) <= 0) return;
#endif

    /* Drain everything currently available from host stdin. */
    uint8_t buf[64];
    for (;;)
    {
        ssize_t r = read(STDIN_FILENO, buf, sizeof(buf));
        if (r > 0)
        {
            for (ssize_t i = 0; i < r; i++) uart_receive(buf[i]);
        }
        else
        {
            if (r < 0 && errno == EAGAIN) break;
            if (r == 0) uart_input_active = 0; /* EOF */
            break;
        }
    }
}

#else /* !CONFIG_ENABLE_UART_INPUT */

void
uart_input_setup(void)
{
}

void
uart_poll_input(void)
{
}

#endif /* CONFIG_ENABLE_UART_INPUT */

#endif /* CONFIG_ENABLE_UART_DEVICE */
