#define PIC1_COMMAND 0x20
#define PIC1_DATA    0x21
#define PIC2_COMMAND 0xA0
#define PIC2_DATA    0xA1
#define PIT_CHANNEL0 0x40
#define PIT_COMMAND  0x43
#define PIT_FREQUENCY 1193182
#include "interrupts.h"

static volatile uint32_t timer_ticks = 0;


/*
 * One entry in the x86 Interrupt Descriptor Table.
 * The CPU uses this structure to determine where an
 * interrupt handler is located.
 */

extern void irq0_stub(void);


typedef struct {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t  zero;
    uint8_t  type_attr;
    uint16_t offset_high;
} __attribute__((packed)) idt_entry_t;


/*
 * Structure passed to the LIDT instruction.
 */
typedef struct {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed)) idt_ptr_t;


/* x86 supports 256 interrupt vectors. */
#define IDT_ENTRIES 256

static idt_entry_t idt[IDT_ENTRIES];
static idt_ptr_t idt_ptr;


/*
 * Set one entry in the IDT.
 */
static void idt_set_gate(
    uint8_t vector,
    uint32_t handler,
    uint16_t selector,
    uint8_t type_attr
) {
    idt[vector].offset_low = handler & 0xFFFF;
    idt[vector].selector = selector;
    idt[vector].zero = 0;
    idt[vector].type_attr = type_attr;
    idt[vector].offset_high = (handler >> 16) & 0xFFFF;
}


/*
 * Load our IDT into the CPU's IDTR register.
 */


static inline void outb(uint16_t port, uint8_t value) {
    __asm__ __volatile__(
        "outb %0, %1"
        :
        : "a"(value), "Nd"(port)
    );
}

static void io_wait(void) {
    outb(0x80, 0);
}




static void idt_load(void) {
    __asm__ __volatile__(
        "lidt %0"
        :
        : "m"(idt_ptr)
    );
}

static void pic_remap(void) {
    uint8_t master_mask;
    uint8_t slave_mask;

    __asm__ __volatile__("inb %1, %0"
                         : "=a"(master_mask)
                         : "Nd"(PIC1_DATA));

    __asm__ __volatile__("inb %1, %0"
                         : "=a"(slave_mask)
                         : "Nd"(PIC2_DATA));

    outb(PIC1_COMMAND, 0x11);
    io_wait();

    outb(PIC2_COMMAND, 0x11);
    io_wait();

    /*
     * Master PIC IRQ0-IRQ7 -> vectors 32-39
     */
    outb(PIC1_DATA, 0x20);
    io_wait();

    /*
     * Slave PIC IRQ8-IRQ15 -> vectors 40-47
     */
    outb(PIC2_DATA, 0x28);
    io_wait();

    /*
     * Tell master PIC that slave is connected on IRQ2.
     */
    outb(PIC1_DATA, 0x04);
    io_wait();

    /*
     * Tell slave PIC its cascade identity is 2.
     */
    outb(PIC2_DATA, 0x02);
    io_wait();

    /*
     * 8086/88 mode.
     */
    outb(PIC1_DATA, 0x01);
    io_wait();

    outb(PIC2_DATA, 0x01);
    io_wait();

    /*
     * Restore previous masks.
     */
    outb(PIC1_DATA, master_mask);
    outb(PIC2_DATA, slave_mask);
}


uint32_t timer_get_ticks(void) {
    return timer_ticks;
}


void irq0_handler_c(void) {
    timer_ticks++;

    /* Send End Of Interrupt to master PIC */
    outb(PIC1_COMMAND, 0x20);
}


void pit_init(uint32_t frequency) {
    if (frequency == 0) {
        return;
    }

    uint32_t divisor = PIT_FREQUENCY / frequency;

    /*
     * PIT channel 0
     * Access mode: low byte then high byte
     * Operating mode: mode 3 (square wave)
     * Binary counting
     */
    outb(PIT_COMMAND, 0x36);

    uint8_t low = (uint8_t)(divisor & 0xFF);
    uint8_t high = (uint8_t)((divisor >> 8) & 0xFF);

    outb(PIT_CHANNEL0, low);
    outb(PIT_CHANNEL0, high);
}


static void pic_unmask_irq0_only(void) {
    /*
     * Master PIC mask:
     * bit = 1 -> masked/disabled
     * bit = 0 -> enabled
     *
     * 11111110b = only IRQ0 enabled.
     */
    outb(PIC1_DATA, 0xFE);

    /*
     * Disable every IRQ on the slave PIC for now.
     */
    outb(PIC2_DATA, 0xFF);
}


void interrupts_init(void) {

    /*
     * Start with an empty IDT.
     */
    for (int i = 0; i < IDT_ENTRIES; i++) {
        idt[i].offset_low = 0;
        idt[i].selector = 0;
        idt[i].zero = 0;
        idt[i].type_attr = 0;
        idt[i].offset_high = 0;
    }

    idt_ptr.limit = sizeof(idt) - 1;
    idt_ptr.base = (uint32_t)&idt;


  /* Remap hardware IRQs to vectors 32-47 */

    pic_remap();

    pic_unmask_irq0_only();

    idt_set_gate(32, (uint32_t)irq0_stub, 0x08, 0x8E);

    /*
     * First we only load the IDT structure.
     */
   
    idt_load();
}


void interrupts_enable(void) {
    __asm__ __volatile__("sti");
}


void interrupts_disable(void) {
    __asm__ __volatile__("cli");
}

