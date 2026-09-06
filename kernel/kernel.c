#include "vga.h"
#include "keyboard.h"
#include "../include/types.h"
#include "process.h"
#include "interrupts.h"
#include "scheduler.h"
#include "thread.h"
#include "mutex.h"
#include "semaphore.h"
#include "pmm.h"
#include "e820.h"
#include "fs.h"

static volatile uint32_t process_a_count = 0;
static volatile uint32_t process_b_count = 0;

static volatile uint32_t thread_a_count = 0;
static volatile uint32_t thread_b_count = 0;

static volatile uint32_t race_shared_count = 0;

static volatile uint32_t mutex_shared_count = 0;
static mutex_t counter_mutex;

static volatile uint32_t semaphore_shared_count = 0;
static semaphore_t counter_semaphore;

#define PC_BUFFER_SIZE 5

static int pc_buffer[PC_BUFFER_SIZE];

static uint32_t pc_in = 0;
static uint32_t pc_out = 0;

static volatile uint32_t produced_count = 0;
static volatile uint32_t consumed_count = 0;

static volatile int last_produced = 0;
static volatile int last_consumed = 0;

static int next_item = 1;

static mutex_t pc_mutex;
static semaphore_t empty_slots;
static semaphore_t full_slots;

static char cat_buffer[(8 * FS_BLOCK_SIZE) + 1];

extern char kernel_end;


/* ---------------------------------------------------------------------------
 * Forward declarations of shell commands
 * --------------------------------------------------------------------------*/
static void cmd_help(void);
static void cmd_clear(void);
static void cmd_about(void);
static void cmd_echo(const char *args);
static void cmd_mem(void);
static void cmd_ticks(void);
static void cmd_counts(void);
static void cmd_kill(const char *args);
static void cmd_threads(void);
static void cmd_pc(void);
static void cmd_meminfo(void);
static void cmd_pmmtest(void);
static void cmd_e820(void);
static void cmd_fsinfo(void);
static void cmd_touch(const char *args);
static void cmd_ls(void);
static void cmd_write(const char *args);
static void cmd_cat(const char *args);
static void cmd_rm(const char *args);


/* ---------------------------------------------------------------------------
 * Utility: minimal string helpers (no libc in a freestanding kernel!)
 * --------------------------------------------------------------------------*/
static int k_strcmp(const char *a, const char *b) {
    while (*a && (*a == *b)) { a++; b++; }
    return (uint8_t)*a - (uint8_t)*b;
}

static int k_strncmp(const char *a, const char *b, size_t n) {
    while (n-- && *a && (*a == *b)) { a++; b++; }
    return n == (size_t)-1 ? 0 : (uint8_t)*a - (uint8_t)*b;
}

static size_t k_strlen(const char *s) {
    size_t n = 0;
    while (s[n]) n++;
    return n;
}

/* Skip leading spaces */
static const char *k_ltrim(const char *s) {
    while (*s == ' ') s++;
    return s;
}


static void k_put_uint(uint32_t value) {
    char buffer[11];
    int i = 0;

    if (value == 0) {
        vga_putchar('0');
        return;
    }

    while (value > 0) {
        buffer[i++] = '0' + (value % 10);
        value /= 10;
    }

    while (i > 0) {
        vga_putchar(buffer[--i]);
    }
}


static int k_parse_uint(const char *str, uint32_t *value) {
    uint32_t result = 0;

    if (str == 0 || *str == '\0') {
        return -1;
    }

    while (*str != '\0') {
        if (*str < '0' || *str > '9') {
            return -1;
        }

        result = result * 10 + (uint32_t)(*str - '0');
        str++;
    }

    *value = result;
    return 0;
}

static void init_physical_memory_from_e820(void) {
    uint16_t count = e820_get_entry_count();
    volatile e820_entry_t *entries = e820_get_entries();

    pmm_init();

    
    for (uint16_t i = 0; i < count; i++) {

        if (entries[i].type != E820_TYPE_USABLE) {
            continue;
        }

        
        if (entries[i].base >= PMM_MAX_MEMORY) {
            continue;
        }

        uint32_t base = (uint32_t)entries[i].base;
        uint32_t length = (uint32_t)entries[i].length;

        
        if (base + length > PMM_MAX_MEMORY ||
            base + length < base) {

            length = PMM_MAX_MEMORY - base;
        }

        pmm_mark_region_free(base, length);
    }

    pmm_mark_region_used( 0x00000000,(uint32_t)&kernel_end);
    pmm_mark_region_used(0x001FF000,PMM_FRAME_SIZE);
}


/* ---------------------------------------------------------------------------
 * Splash Screen
 * --------------------------------------------------------------------------*/
static void print_splash(void) {
    vga_clear(VGA_BLACK);

    /* Top banner box */
    vga_draw_box(0, 0, 7, 80, VGA_LIGHT_MAGENTA);

    vga_set_cursor(1, 2);
    vga_puts_color("  SENG21213-OS  |  Computer Architecture & Operating Systems",
                   VGA_YELLOW, VGA_BLACK);

    vga_set_cursor(2, 2);
    vga_puts_color("  Stage 0: Kernel Foundations", VGA_LIGHT_CYAN, VGA_BLACK);

    vga_set_cursor(3, 2);
    vga_puts_color("  Faculty of Engineering – Department of Software Engineering",
                   VGA_LIGHT_GREY, VGA_BLACK);

    vga_set_cursor(4, 2);
    vga_puts_color("  Built by students, for students.  Type 'help' to begin.",
                   VGA_LIGHT_GREEN, VGA_BLACK);

    vga_set_cursor(5, 2);
    vga_puts_color("  CPU: i686 (32-bit Protected Mode)  |  Display: VGA 80x25",
                   VGA_DARK_GREY, VGA_BLACK);

    vga_set_cursor(8, 0);
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    vga_puts("  Welcome! This kernel was compiled from source and booted entirely\n");
    vga_puts("  from bare metal. There is no Linux or Windows underneath – only\n");
    vga_puts("  the code you and your team write.\n");
    vga_puts("\n");
    vga_puts("  Assignment milestones to implement:\n");
    vga_puts_color("    [L09] ", VGA_YELLOW, VGA_BLACK);
    vga_puts("Process Management  – PCB, ready queue, round-robin scheduler\n");
    vga_puts_color("    [L10] ", VGA_YELLOW, VGA_BLACK);
    vga_puts("Threads & Sync      – kernel threads, mutex, semaphore\n");
    vga_puts_color("    [L11] ", VGA_YELLOW, VGA_BLACK);
    vga_puts("Memory Management   – physical page allocator, virtual memory\n");
    vga_puts_color("    [L12] ", VGA_YELLOW, VGA_BLACK);
    vga_puts("File System         – RAM disk, FAT-like directory structure\n");
    vga_puts("\n");
}

/* ---------------------------------------------------------------------------
 * Shell command implementations
 * --------------------------------------------------------------------------*/
static void cmd_help(void) {
    vga_puts_color("\n  SENG21213-OS Shell Commands\n", VGA_YELLOW, VGA_BLACK);
    vga_puts("  ─────────────────────────────────────────────\n");
    vga_puts("  help    – Show this help message\n");
    vga_puts("  clear   – Clear the screen\n");
    vga_puts("  about   – About this OS and course\n");
    vga_puts("  echo    – Echo text to screen\n");
    vga_puts("  mem     – Memory map (stub)\n");
    vga_puts_color("\n  Milestones (to implement):\n", VGA_LIGHT_CYAN, VGA_BLACK);
    vga_puts("  ps      – [L09] List processes\n");
    vga_puts("  kill    – [L09] Terminate a process\n");
    vga_puts("  threads – [L10] List kernel threads\n");
    vga_puts("  pc      - [L11] Producer-consumer status\n");
    vga_puts("  free    – [L12] Show free memory\n");
    vga_puts("  ls      – [L13] List files\n");
    vga_puts("  cat     – [L14] Print file contents\n\n");
    vga_puts("  meminfo - [L15] Physical memory statistics\n");
    vga_puts("  pmmtest - [L16] Test frame allocation/free\n");
    vga_puts("  e820    - [L17] Show BIOS physical memory map\n");
    vga_puts("  fsinfo  - [L18] RAM disk filesystem information\n");
    vga_puts("  touch   - [L19] Create an empty file\n");
    vga_puts("  ls      - [L20] List files in RAM disk\n");
    vga_puts("  write   - [L21] Write text to a file\n");
    vga_puts("  cat     - [L22] Display file contents\n");
    vga_puts("  rm      - [L23] Delete a file\n");
    
}

static void cmd_clear(void) {
    vga_clear(VGA_BLACK);
}

static void cmd_about(void) {
    vga_puts_color("\n  About SENG21213-OS\n", VGA_LIGHT_CYAN, VGA_BLACK);
    vga_puts("  ─────────────────────────────────────────────\n");
    vga_puts("  Architecture : x86 (i686), 32-bit Protected Mode\n");
    vga_puts("  Bootloader   : Custom MBR (NASM)\n");
    vga_puts("  Kernel       : Freestanding C (GCC, no libc)\n");
    vga_puts("  VM Target    : QEMU (qemu-system-i386)\n");
    vga_puts("  Course       : SENG 21213 – Sem 2\n");
    vga_puts("  Reference    : Stallings, OS: Internals & Design Principles\n\n");
}

static void cmd_echo(const char *args) {
    vga_puts("  ");
    vga_puts(args);
    vga_puts("\n");
}

static void cmd_mem(void) {
    /* Stage 0 stub – students implement the real PMM in Lecture 11 */
    vga_puts_color("\n  Memory Map (stub – implement PMM in Lecture 11)\n",
                   VGA_LIGHT_CYAN, VGA_BLACK);
    vga_puts("  ─────────────────────────────────────────────\n");
    vga_puts("  0x00000000 – 0x000FFFFF  :  First 1 MB (reserved/BIOS)\n");
    vga_puts("  0x00100000 – 0x00EFFFFF  :  Extended memory (usable ~14 MB)\n");
    vga_puts("  0x00F00000 – 0x00FFFFFF  :  BIOS / ROM area\n");
    vga_puts("  0xB8000    – 0xBFFFF     :  VGA frame buffer\n");
    vga_puts_color("\n  TODO: Use BIOS int 0x15, EAX=0xE820 to get real memory map\n\n",
                   VGA_YELLOW, VGA_BLACK);
}


static void cmd_ps(void) {
    pcb_t *process = process_get_list();

    vga_puts_color("\n  PID    STATE        NAME\n",
                   VGA_LIGHT_CYAN, VGA_BLACK);
    vga_puts("  -----------------------------\n");

    if (process == 0) {
        vga_puts("  No processes found.\n");
        return;
    }

    while (process != 0) {
        vga_puts("  ");

        k_put_uint(process->pid);

        vga_puts("      ");
        vga_puts(process_state_string(process->state));

        vga_puts("        ");

        if (process->name != 0) {
            vga_puts(process->name);
        } else {
            vga_puts("(unnamed)");
        }

        vga_puts("\n");

        process = process->next;
    }
}


static void cmd_threads(void) {
    thread_t *thread = thread_get_list();

    vga_puts_color("\n  TID    PID    STATE        NAME\n",
                   VGA_LIGHT_CYAN, VGA_BLACK);

    vga_puts("  ----------------------------------\n");

    if (thread == 0) {
        vga_puts("  No threads found.\n");
        return;
    }

    while (thread != 0) {
        vga_puts("  ");

        k_put_uint(thread->tid);

        vga_puts("      ");

        k_put_uint(thread->owner_pid);

        vga_puts("      ");

        vga_puts(thread_state_string(thread->state));

        vga_puts("        ");

        if (thread->name != 0) {
            vga_puts(thread->name);
        } else {
            vga_puts("(unnamed)");
        }

        vga_puts("\n");

        thread = thread->next;
    }
}


static void cmd_pc(void) {
    uint32_t produced;
    uint32_t consumed;
    int produced_item;
    int consumed_item;

  

    mutex_lock(&pc_mutex);

    produced = produced_count;
    consumed = consumed_count;
    produced_item = last_produced;
    consumed_item = last_consumed;

    mutex_unlock(&pc_mutex);

    vga_puts_color("\n  Producer-Consumer Status\n",
                   VGA_LIGHT_CYAN, VGA_BLACK);

    vga_puts("  ------------------------------\n");

    vga_puts("  Produced       : ");
    k_put_uint(produced);
    vga_puts("\n");

    vga_puts("  Consumed       : ");
    k_put_uint(consumed);
    vga_puts("\n");

    vga_puts("  Last produced  : ");
    k_put_uint((uint32_t)produced_item);
    vga_puts("\n");

    vga_puts("  Last consumed  : ");
    k_put_uint((uint32_t)consumed_item);
    vga_puts("\n");

    vga_puts("  Buffer used    : ");
    k_put_uint(produced - consumed);
    vga_puts(" / ");
    k_put_uint(PC_BUFFER_SIZE);
    vga_puts("\n");
}


static void cmd_ticks(void) {
    vga_puts("  Timer ticks: ");
    k_put_uint(timer_get_ticks());
    vga_puts("\n");
}


static void cmd_counts(void) {
    vga_puts("  Process A count : ");
    k_put_uint(process_a_count);
    vga_puts("\n");

    vga_puts("  Process B count : ");
    k_put_uint(process_b_count);
    vga_puts("\n");

    vga_puts("  Thread A count  : ");
    k_put_uint(thread_a_count);
    vga_puts("\n");

    vga_puts("  Thread B count  : ");
    k_put_uint(thread_b_count);
    vga_puts("\n");

    vga_puts("  Mutex shared    : ");
    k_put_uint(mutex_shared_count);
    vga_puts("\n");

    vga_puts("  Race shared     : ");
    k_put_uint(race_shared_count);
    vga_puts("\n");

    vga_puts("  Semaphore shared: ");
    k_put_uint(semaphore_shared_count);
    vga_puts("\n");
}


static void cmd_kill(const char *args) {
    uint32_t pid;

    args = k_ltrim(args);

    if (args == 0 || *args == '\0') {
        vga_puts("  Usage: kill <pid>\n");
        return;
    }

    if (k_parse_uint(args, &pid) != 0) {
        vga_puts("  Invalid PID.\n");
        return;
    }

    pcb_t *process = process_find(pid);

    if (process == 0) {
        vga_puts("  Process not found.\n");
        return;
    }

    if (process == process_current()) {
        vga_puts("  Cannot kill the currently running process.\n");
        return;
    }

    if (process_kill(pid) == 0) {
        vga_puts("  Process ");
        k_put_uint(pid);
        vga_puts(" terminated.\n");
    } else {
        vga_puts("  Failed to terminate process.\n");
    }
}

    static void cmd_meminfo(void) {
    vga_puts_color("\n  Physical Memory Manager\n",VGA_LIGHT_CYAN, VGA_BLACK);

    vga_puts("  ------------------------------\n");

    vga_puts("  Total frames : ");
    k_put_uint(pmm_get_total_frames());
    vga_puts("\n");

    vga_puts("  Used frames  : ");
    k_put_uint(pmm_get_used_frames());
    vga_puts("\n");

    vga_puts("  Free frames  : ");
    k_put_uint(pmm_get_free_frames());
    vga_puts("\n");

    vga_puts("  Frame size   : 4096 bytes\n");
  }


    static void cmd_pmmtest(void) {
     uint32_t before_free;
     uint32_t after_alloc;
     uint32_t after_free;
     uint32_t frame;

    vga_puts_color("\n  PMM Allocation Test\n",
                   VGA_LIGHT_CYAN, VGA_BLACK);

    vga_puts("  ------------------------------\n");

    before_free = pmm_get_free_frames();

    vga_puts("  Free before allocate : ");
    k_put_uint(before_free);
    vga_puts("\n");

    frame = pmm_alloc_frame();

    if (frame == 0) {
        vga_puts("  Allocation failed.\n");
        return;
    }

    after_alloc = pmm_get_free_frames();

    vga_puts("  Allocated frame addr : ");
    k_put_uint(frame);
    vga_puts("\n");

    vga_puts("  Free after allocate  : ");
    k_put_uint(after_alloc);
    vga_puts("\n");

    pmm_free_frame(frame);

    after_free = pmm_get_free_frames();

    vga_puts("  Free after free      : ");
    k_put_uint(after_free);
    vga_puts("\n");

    if (after_alloc + 1 == before_free &&
        after_free == before_free) {

        vga_puts_color("  PMM test: PASS\n",
                       VGA_LIGHT_GREEN, VGA_BLACK);
    } else {
        vga_puts_color("  PMM test: FAIL\n",
                       VGA_LIGHT_RED, VGA_BLACK);
    }
}


static void cmd_fsinfo(void) {
    fs_superblock_t *sb = fs_get_superblock();

    vga_puts_color("\n  RAM Disk File System\n",
                   VGA_LIGHT_CYAN, VGA_BLACK);

    vga_puts("  ------------------------------\n");

    vga_puts("  Total blocks : ");
    k_put_uint(sb->total_blocks);
    vga_puts("\n");

    vga_puts("  Free blocks  : ");
    k_put_uint(sb->free_blocks);
    vga_puts("\n");

    vga_puts("  Total inodes : ");
    k_put_uint(sb->total_inodes);
    vga_puts("\n");

    vga_puts("  Free inodes  : ");
    k_put_uint(sb->free_inodes);
    vga_puts("\n");

    vga_puts("  Block size   : 512 bytes\n");
    vga_puts("  RAM disk     : 1 MB\n");
}


static void cmd_touch(const char *args) {
    args = k_ltrim(args);

    if (args == 0 || *args == '\0') {
        vga_puts("  Usage: touch <filename>\n");
        return;
    }

    int result = fs_create(args);

    if (result == 0) {
        vga_puts("  Created file: ");
        vga_puts(args);
        vga_puts("\n");
    }
    else if (result == -2) {
        vga_puts("  File already exists.\n");
    }
    else if (result == -3) {
        vga_puts("  No free inodes available.\n");
    }
    else {
        vga_puts("  Invalid filename.\n");
    }
}



static void cmd_ls(void) {
    uint32_t file_count = 0;

    vga_puts_color("\n  Files\n",
                   VGA_LIGHT_CYAN, VGA_BLACK);

    vga_puts("  ------------------------------------------\n");
    vga_puts("  NAME                            SIZE\n");
    vga_puts("  ------------------------------------------\n");

    for (uint32_t i = 0; i < FS_MAX_INODES; i++) {
        fs_inode_t *inode = fs_get_inode(i);

        if (inode == 0 || !inode->used) {
            continue;
        }

        vga_puts("  ");
        vga_puts(inode->name);

        uint32_t length = 0;
        while (inode->name[length] != '\0') {
            length++;
        }

        while (length < 32) {
            vga_puts(" ");
            length++;
        }

        k_put_uint(inode->size);
        vga_puts(" bytes\n");

        file_count++;
    }

    if (file_count == 0) {
        vga_puts("  <no files>\n");
    }

    vga_puts("\n  Total files: ");
    k_put_uint(file_count);
    vga_puts("\n");
}


static void cmd_write(const char *args) {
    args = k_ltrim(args);

    if (args == 0 || *args == '\0') {
        vga_puts("  Usage: write <filename> <text>\n");
        return;
    }

    /*
     * write hello.txt Hello world
     * into:
     * filename = "hello.txt"
     * data     = "Hello world"
     */

    const char *space = args;

    while (*space != '\0' && *space != ' ') {
        space++;
    }

    if (*space == '\0') {
        vga_puts("  Usage: write <filename> <text>\n");
        return;
    }


    /* Copy filename into a temporary buffer. */

    char filename[FS_MAX_FILENAME];
    uint32_t i = 0;

    while (args[i] != '\0' &&
           args[i] != ' ' &&
           i < FS_MAX_FILENAME - 1) {

        filename[i] = args[i];
        i++;
    }

    filename[i] = '\0';


    /* Skip spaces before file contents.*/

    const char *data = space;

    while (*data == ' ') {
        data++;
    }

    int result = fs_write(filename, data);

    if (result == 0) {
        vga_puts("  Written to: ");
        vga_puts(filename);
        vga_puts("\n");
    }
    else if (result == -2) {
        vga_puts("  File not found.\n");
    }
    else if (result == -3) {
        vga_puts("  File too large. Maximum size is 4096 bytes.\n");
    }
    else if (result == -4) {
        vga_puts("  Not enough free blocks.\n");
    }
    else {
        vga_puts("  Write failed.\n");
    }
}



static void cmd_cat(const char *args) {
    args = k_ltrim(args);

    if (args == 0 || *args == '\0') {
        vga_puts("  Usage: cat <filename>\n");
        return;
    }

    int result = fs_read(
        args,
        cat_buffer,
        sizeof(cat_buffer)
    );

    if (result == -2) {
        vga_puts("  File not found.\n");
        return;
    }

    if (result < 0) {
        vga_puts("  Failed to read file.\n");
        return;
    }

    vga_puts("\n");

    if (result == 0) {
        vga_puts("  <empty file>");
    } else {
        vga_puts(cat_buffer);
    }

    vga_puts("\n");
}


static void cmd_rm(const char *args) {
    args = k_ltrim(args);

    if (args == 0 || *args == '\0') {
        vga_puts("  Usage: rm <filename>\n");
        return;
    }

    int result = fs_delete(args);

    if (result == 0) {
        vga_puts("  Deleted: ");
        vga_puts(args);
        vga_puts("\n");
    }
    else if (result == -2) {
        vga_puts("  File not found.\n");
    }
    else {
        vga_puts("  Delete failed.\n");
    }
}


static void cmd_e820(void) {
    uint16_t count = e820_get_entry_count();
    volatile e820_entry_t *entries = e820_get_entries();

    vga_puts_color("\n  BIOS E820 Memory Map\n",
                   VGA_LIGHT_CYAN, VGA_BLACK);

    vga_puts("  ------------------------------------------\n");

    vga_puts("  Entry count: ");
    k_put_uint(count);
    vga_puts("\n\n");

    for (uint16_t i = 0; i < count; i++) {

        /*
         * Our current kernel is 32-bit, so for this teaching OS
         * we display the lower 32 bits of base and length.
         */

        uint32_t base_low =
            (uint32_t)(entries[i].base & 0xFFFFFFFFu);

        uint32_t length_low =
            (uint32_t)(entries[i].length & 0xFFFFFFFFu);

        vga_puts("  Entry ");
        k_put_uint(i);

        vga_puts(": base=");
        k_put_uint(base_low);

        vga_puts(" length=");
        k_put_uint(length_low);

        vga_puts(" type=");
        k_put_uint(entries[i].type);

        if (entries[i].type == E820_TYPE_USABLE) {
            vga_puts(" [USABLE]");
        } else {
            vga_puts(" [RESERVED]");
        }

        vga_puts("\n");
    }
}


/* ---------------------------------------------------------------------------
 * Shell process
 * --------------------------------------------------------------------------*/
static char  shell_buf[256];
static char  prompt[] = "\n  ksh> ";

static void shell_run(void) {
    vga_puts_color("\n  Kernel Shell ready. Type 'help' for commands.\n",
                   VGA_LIGHT_GREEN, VGA_BLACK);

    while (true) {
        vga_puts_color(prompt, VGA_LIGHT_GREEN, VGA_BLACK);
        kb_readline(shell_buf, sizeof(shell_buf));

        /* Trim leading whitespace */
        const char *cmd = k_ltrim(shell_buf);
        if (k_strlen(cmd) == 0) continue;

        /* Dispatch */
        if (k_strcmp(cmd, "help")  == 0) { cmd_help();  continue; }
        if (k_strcmp(cmd, "clear") == 0) { cmd_clear(); continue; }
        if (k_strcmp(cmd, "about") == 0) { cmd_about(); continue; }
        if (k_strcmp(cmd, "mem")   == 0) { cmd_mem();   continue; }
	if (k_strcmp(cmd, "ticks") == 0) { cmd_ticks(); continue; }

        if (k_strncmp(cmd, "echo ", 5) == 0) {
            cmd_echo(k_ltrim(cmd + 5));
            continue;
        }

	if (k_strcmp(cmd, "ps") == 0) {
    	cmd_ps();
    	continue;
	}

	if (k_strcmp(cmd, "threads") == 0) {
   	cmd_threads();
    	continue;
	}

	if (k_strcmp(cmd, "pc") == 0) {
    	cmd_pc();
    	continue;
	}

	if (k_strncmp(cmd, "kill ", 5) == 0) {
    	cmd_kill(cmd + 5);
    	continue;
	}

	if (k_strcmp(cmd, "kill") == 0) {
    	cmd_kill("");
    	continue;
	}
	

	if (k_strcmp(cmd, "counts") == 0) {
    	cmd_counts();
    	continue;
	}

	if (k_strcmp(cmd, "meminfo") == 0) {
    	cmd_meminfo();
    	continue;
	}

	if (k_strcmp(cmd, "pmmtest") == 0) {
    	cmd_pmmtest();
    	continue;
	}

	if (k_strcmp(cmd, "e820") == 0) {
    	cmd_e820();
    	continue;
	}

	if (k_strcmp(cmd, "fsinfo") == 0) {
   	cmd_fsinfo();
    	continue;
	}


	if (k_strcmp(cmd, "touch") == 0) {
    	cmd_touch("");
    	continue;
	}

	
	if (cmd[0] == 't' &&
    	cmd[1] == 'o' &&
    	cmd[2] == 'u' &&
    	cmd[3] == 'c' &&
    	cmd[4] == 'h' &&
    	cmd[5] == ' ') {

   	cmd_touch(cmd + 6);
    	continue;
	}


	if (k_strcmp(cmd, "ls") == 0) {
    	cmd_ls();
    	continue;
	}

	
	if (k_strcmp(cmd, "write") == 0) {
    	cmd_write("");
    	continue;
	}

	if (cmd[0] == 'w' &&
    	cmd[1] == 'r' &&
    	cmd[2] == 'i' &&
    	cmd[3] == 't' &&
    	cmd[4] == 'e' &&
    	cmd[5] == ' ') {

    	cmd_write(cmd + 6);
    	continue;
	}	

	if (k_strcmp(cmd, "cat") == 0) {
    	cmd_cat("");
    	continue;
	}

	if (cmd[0] == 'c' &&
    	cmd[1] == 'a' &&
    	cmd[2] == 't' &&
    	cmd[3] == ' ') {

    	cmd_cat(cmd + 4);
    	continue;
	}

	if (k_strcmp(cmd, "rm") == 0) {
    	cmd_rm("");
    	continue;
	}

	if (cmd[0] == 'r' &&
    	cmd[1] == 'm' &&
    	cmd[2] == ' ') {

    	cmd_rm(cmd + 3);
    	continue;
	}

        /* Milestone stubs */
        if (k_strcmp(cmd, "kill")    == 0 ||
            k_strcmp(cmd, "threads") == 0 ||
            k_strcmp(cmd, "free")    == 0 ||
            k_strcmp(cmd, "ls")      == 0 ||
            k_strcmp(cmd, "cat")     == 0) {
            vga_puts_color("  [TODO] This command is not yet implemented.\n",
                           VGA_YELLOW, VGA_BLACK);
            vga_puts("  Implement it as part of your lecture assignment.\n");
            continue;
        }

	

        vga_puts_color("  Unknown command: ", VGA_LIGHT_RED, VGA_BLACK);
        vga_puts(cmd);
        vga_puts("\n  Type 'help' for a list of commands.\n");
    	}
	}



	static void process_a(void) {
    	while (1) {
        process_a_count++;
   	}
	}

	static void process_b(void) {
    	while (1) {
        process_b_count++;
    	}
	}




/*
 * Intentionally unsafe increment used only
 * to demonstrate a race condition.
 */
static void unsafe_race_increment(void) {
    uint32_t temp = race_shared_count;

    /*
     * Widen the race window so the PIT has a better
     * chance of switching threads before the write.
     */
    for (volatile uint32_t i = 0; i < 50000; i++) {
        /* intentional delay */
    }

    race_shared_count = temp + 1;
}

/*thread a*/

static void thread_a(void) {
    while (1) {

        /* Intentionally unsafe */
        unsafe_race_increment();

        /* Protected by mutex */
        mutex_lock(&counter_mutex);

        thread_a_count++;
        mutex_shared_count++;

        mutex_unlock(&counter_mutex);


        /* Protected by semaphore */
        semaphore_wait(&counter_semaphore);

        semaphore_shared_count++;

        semaphore_signal(&counter_semaphore);
    }
}


/*thread b*/
static void thread_b(void) {
    while (1) {

        /* Intentionally unsafe */
        unsafe_race_increment();

        /* Protected by mutex */
        mutex_lock(&counter_mutex);

        thread_b_count++;
        mutex_shared_count++;

        mutex_unlock(&counter_mutex);


        /* Protected by semaphore */
        semaphore_wait(&counter_semaphore);

        semaphore_shared_count++;

        semaphore_signal(&counter_semaphore);
    }
}



static void producer_thread(void) {
    while (1) {

        /*
         * Wait until at least one empty
         * buffer slot is available.
         */
        semaphore_wait(&empty_slots);

        /*
         * Protect the shared buffer.
         */
        mutex_lock(&pc_mutex);

        int item = next_item++;

        pc_buffer[pc_in] = item;
        pc_in = (pc_in + 1) % PC_BUFFER_SIZE;

        last_produced = item;
        produced_count++;

        mutex_unlock(&pc_mutex);

        /*
         * One more full slot is now available.
         */
        semaphore_signal(&full_slots);
    }
}


static void consumer_thread(void) {
    while (1) {

        /*
         * Wait until there is an item
         * available to consume.
         */
        semaphore_wait(&full_slots);

        mutex_lock(&pc_mutex);

        int item = pc_buffer[pc_out];

        pc_out = (pc_out + 1) % PC_BUFFER_SIZE;

        last_consumed = item;
        consumed_count++;

        mutex_unlock(&pc_mutex);

        /*
         * One more empty slot is now available.
         */
        semaphore_signal(&empty_slots);
    }
}




/* ---------------------------------------------------------------------------
 * Kernel entry point – called from kernel_entry.asm
 * --------------------------------------------------------------------------*/
void kernel_main(void) {
    vga_init();
    kb_init();

    process_init();
    thread_init();
    scheduler_init();
   
    
    init_physical_memory_from_e820();
    fs_init();	

    mutex_init(&counter_mutex);
    semaphore_init(&counter_semaphore, 1);

    mutex_init(&pc_mutex);

    semaphore_init(&empty_slots, PC_BUFFER_SIZE);
    semaphore_init(&full_slots, 0);
    


    pcb_t *proc_a = process_create("process_a", process_a);
    pcb_t *proc_b = process_create("process_b", process_b);


    thread_create("thread_a", thread_a, proc_a->pid);
    thread_create("thread_b", thread_b, proc_b->pid);

    thread_create("producer", producer_thread, proc_a->pid);
    thread_create("consumer", consumer_thread, proc_b->pid);

    
    pcb_t *shell_process = process_create("shell", shell_run);

    shell_process->state = PROCESS_RUNNING;
    process_set_current(shell_process);

    interrupts_init();


    pit_init(100);
    interrupts_enable();

    print_splash();
    shell_run();

    __asm__ __volatile__("hlt");
}