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
#define IER_ETBEI (1u << 1) /* enable transmitter-empty interrupt */

/* LSR bits. */
#define LSR_DR (1u << 0)   /* data ready */
#define LSR_THRE (1u << 5) /* transmit holding register empty */
#define LSR_TEMT (1u << 6) /* transmitter empty */

/* IIR values (no FIFO state modelled).  The 16550 reports the highest-priority
 * pending interrupt cause in bits [5:1] with bit 0 clear when one is pending:
 *   0x04 = received data available (RX, highest)
 *   0x02 = transmitter holding register empty (TX, next)
 *   0x01 = no interrupt pending
 * Linux's 8250 driver drives its buffered TX from the transmitter-empty
 * interrupt, so without IIR 0x02 (and the corresponding IRQ line assertion)
 * the console would transmit the first byte and then stall forever. */
#define IIR_NOPEND 0x01
#define IIR_RXDATA 0x04 /* received data available */
#define IIR_THRE 0x02   /* transmitter holding register empty */

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

    /* Transmitter model: a THR write loads the transmitter, which clears THRE
     * for TX_SHIFT_TICKS emulated instructions and then re-asserts it (and,
     * with IER.ETBEI, raises the transmitter-empty interrupt).  This paces
     * the guest's interrupt-driven tty output so it never reads its own
     * transmit FIFO faster than the data is produced. */
    int thr_busy;
    unsigned thr_ticks;
} u;

/* How many emulated instructions one transmitted byte keeps THRE clear.  Only
 * used to pace the transmitter-empty interrupt; the exact value is not timing
 * critical, it just must be long enough that a 16-byte tty burst (a handful
 * of instructions) completes before the interrupt fires. */
#define TX_SHIFT_TICKS 64u

static void
uart_update_irq(void)
{
    int want = 0;
#ifdef CONFIG_ENABLE_PLIC_DEVICE
    /* RX: data available with the receive interrupt enabled.  TX: the
     * transmitter holding register is empty (not busy) and the transmitter-
     * empty interrupt is enabled. */
    int rx_pend = (u.rx_count > 0) && (u.ier & IER_ERBI);
    int tx_pend = !u.thr_busy && (u.ier & IER_ETBEI);
    want = rx_pend || tx_pend;
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

/* Advance the transmitter: after TX_SHIFT_TICKS instructions the loaded byte
 * has "shifted out", THRE re-asserts and the TX interrupt fires.  Called once
 * per emulated instruction from the run loop. */
void
uart_tick(void)
{
    if (u.thr_busy)
    {
        if (--u.thr_ticks == 0)
        {
            u.thr_busy = 0;
            uart_update_irq();
        }
    }
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

static uint32_t uart_read_impl(uint32_t offset);

uint32_t
uart_read(uint32_t offset)
{
    return uart_read_impl(offset);
}

static uint32_t
uart_read_impl(uint32_t offset)
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
        if (!u.thr_busy && (u.ier & IER_ETBEI)) return IIR_THRE;
        return IIR_NOPEND;
    case UART_LCR:
        return u.lcr;
    case UART_MCR:
        return u.mcr;
    case UART_LSR:
        return (u.thr_busy ? 0 : (LSR_THRE | LSR_TEMT))
               | (u.rx_count > 0 ? LSR_DR : 0);
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
        /* Loading the THR clears THRE (the TX interrupt deasserts); the
         * transmitter re-asserts it TX_SHIFT_TICKS instructions later. */
        u.thr_busy = 1;
        u.thr_ticks = TX_SHIFT_TICKS;
        uart_update_irq();
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
#include <signal.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

#if defined(__linux__)
#include <sys/epoll.h>
#define UART_INPUT_EPOLL 1
#else
#include <poll.h>
#endif

/* Console quit escape: Ctrl-] (0x1d) followed by x tells the emulator to leave
 * console mode and exit.  This is the telnet escape convention used by serial
 * consoles; a normal shell never produces this byte pair, so it is safe to
 * intercept even though the guest owns the terminal. */
#define UART_ESC_CHAR 0x1du
#define UART_ESC_QUIT 'x'

static int uart_input_active = 0;

/* A Ctrl-] that was the last byte of a read batch, held back for one poll so
 * an 'x' arriving in the next batch still completes the quit escape.  Only
 * ever set in raw (console) mode, where the guest has not yet seen the byte. */
static int uart_esc_held = 0;

/* Host terminal state for raw (console) mode -- only engaged for an
 * interactive TTY, and always restored before the emulator exits. */
static int uart_terminal_raw = 0;
static struct termios uart_saved_termios;

/* Set by the SIGINT/SIGTERM/SIGQUIT/SIGHUP handler; the run loop polls it so
 * the terminal is always restored from the normal (non-signal) context. */
static volatile sig_atomic_t uart_quit_requested = 0;

static void
uart_input_restore_terminal(void)
{
    if (uart_terminal_raw)
    {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &uart_saved_termios);
        uart_terminal_raw = 0;
    }
}

static void
uart_input_signal(int sig)
{
    (void)sig;
    uart_quit_requested = 1; /* async-signal-safe: volatile sig_atomic_t */
}

void
uart_input_cleanup(void)
{
    uart_input_restore_terminal();
    uart_input_active = 0;
}

void
uart_input_setup(int raw_mode)
{
    /* Non-blocking stdin so the loop never stalls on a host read. */
    int fl = fcntl(STDIN_FILENO, F_GETFL, 0);
    if (fl < 0) return;
    fcntl(STDIN_FILENO, F_SETFL, fl | O_NONBLOCK);
    uart_input_active = 1;

    if (!raw_mode) return;

    /* Console mode: hand the host terminal to the guest.  Put the TTY into raw
     * mode -- no canonical line buffering (keystrokes reach the guest the
     * moment they are typed instead of being held until Enter) and no echo
     * (the guest controls everything that appears on screen).  ISIG is cleared
     * so ^C etc. are delivered to the guest as plain bytes; the Ctrl-] x
     * escape is the way out of console mode. */
    if (isatty(STDIN_FILENO) != 1
        || tcgetattr(STDIN_FILENO, &uart_saved_termios) != 0)
    {
        return; /* piped / non-interactive input: nothing to switch */
    }

    struct termios raw = uart_saved_termios;
    raw.c_iflag
        &= ~(BRKINT | INPCK | ISTRIP | ICRNL | INLCR | IGNCR | IXON | IXOFF);
    raw.c_oflag &= ~OPOST; /* guest sends its own \r\n */
    raw.c_cflag |= CS8;
    raw.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) != 0) return;

    uart_terminal_raw = 1;
    atexit(uart_input_cleanup);
    signal(SIGINT, uart_input_signal);
    signal(SIGTERM, uart_input_signal);
    signal(SIGQUIT, uart_input_signal);
    signal(SIGHUP, uart_input_signal);

    fprintf(stderr,
            "[riscvemu] UART console: raw mode, press Ctrl-] then x to exit\n");
}

int
uart_poll_input(void)
{
    if (!uart_input_active) return uart_quit_requested != 0;
    if (uart_quit_requested) return 1;

#if UART_INPUT_EPOLL
    static int epfd = -1;
    if (epfd < 0)
    {
        epfd = epoll_create1(0);
        if (epfd < 0)
        {
            uart_input_active = 0;
            return 0;
        }
        struct epoll_event ev;
        memset(&ev, 0, sizeof(ev));
        ev.events = EPOLLIN;
        ev.data.fd = STDIN_FILENO;
        if (epoll_ctl(epfd, EPOLL_CTL_ADD, STDIN_FILENO, &ev) < 0)
        {
            uart_input_active = 0;
            return 0;
        }
    }

    struct epoll_event ev;
    int n = epoll_wait(epfd, &ev, 1, 0); /* timeout 0: never block the loop */
    if (n <= 0) return 0;
#else
    struct pollfd pfd;
    pfd.fd = STDIN_FILENO;
    pfd.events = POLLIN;
    pfd.revents = 0;
    if (poll(&pfd, 1, 0) <= 0) return 0;
#endif

    /* Drain everything currently available from host stdin.  In console mode a
     * Ctrl-] (0x1d) followed by x or X is the quit escape and is not queued;
     * any other byte (including a lone Ctrl-]) passes through untouched because
     * the guest owns the terminal.  A Ctrl-] at the very end of a batch is held
     * for one poll so keystrokes split across reads still work. */
    uint8_t buf[64];
    for (;;)
    {
        ssize_t r = read(STDIN_FILENO, buf, sizeof(buf));
        if (r > 0)
        {
            ssize_t i = 0;
            if (uart_esc_held)
            {
                uart_esc_held = 0;
                if (buf[0] == UART_ESC_QUIT || buf[0] == 'X')
                {
                    uart_quit_requested = 1;
                    break;
                }
                uart_receive(UART_ESC_CHAR); /* held Ctrl-] not followed by x */
            }
            for (; i < r && !uart_quit_requested; i++)
            {
                if (uart_terminal_raw && buf[i] == UART_ESC_CHAR)
                {
                    if (i + 1 < r)
                    {
                        if (buf[i + 1] == UART_ESC_QUIT || buf[i + 1] == 'X')
                        {
                            uart_quit_requested = 1;
                            break;
                        }
                        /* Ctrl-] not followed by x: pass the byte through. */
                        uart_receive(buf[i]);
                    }
                    else
                    {
                        uart_esc_held = 1; /* wait one poll for the 'x' */
                    }
                    continue;
                }
                uart_receive(buf[i]);
            }
            if (uart_quit_requested) break;
        }
        else
        {
            if (r < 0 && errno == EAGAIN) break;
            if (r == 0) uart_input_active = 0; /* EOF */
            break;
        }
    }
    return uart_quit_requested != 0;
}

#else /* !CONFIG_ENABLE_UART_INPUT */

void
uart_input_setup(int raw_mode)
{
    (void)raw_mode;
}

void
uart_input_cleanup(void)
{
}

int
uart_poll_input(void)
{
    return 0;
}

#endif /* CONFIG_ENABLE_UART_INPUT */

#endif /* CONFIG_ENABLE_UART_DEVICE */
