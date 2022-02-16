#include "qemu/osdep.h"
#include "hw/qdev-core.h"
#include "hw/core/cpu.h"
#include "hw/pci/pci.h"
#include "libqos/qgraph.h"
#include "libqos/libqtest.h"
#include "sysemu/qtest.h"
#include "sysemu/sysemu.h"
#include "exec/memory.h"
#include "qemu/main-loop.h"
#include <wordexp.h>

// static int locate_fuzz_memory_regions(Object *child, void *opaque)
// {
//     if (object_dynamic_cast(child, TYPE_MEMORY_REGION))
//     {
//         MemoryRegion *mr = MEMORY_REGION(child);
//         if ((memory_region_is_ram(mr) ||
//              memory_region_is_ram_device(mr) ||
//              memory_region_is_rom(mr)) == false)
//         {
//             const char *name = object_get_canonical_path_component(child);
//         }
//     }
//     return 0;
// }

static int list_fuzzable_objects(Object *child, void *opaque)
{
    // TODO: What can we fuzz?
    //
    if (object_dynamic_cast(child, TYPE_PCI_DEVICE))
    {
        PCIDevice *device = PCI_DEVICE(child);
        g_print("PCIDevice(%s) { }\n",
                device->name);

        return 0;
    }

    // There are several types of memory but we are only interested in those that are
    // handled by MemoryRegionOps. I was able to identify three types but there could
    // be more of them.
    //
    // A MemoryRegion has an MemoryRegionOps pointer to some handlers but even though
    // the pointer may not be null, there could be no handlers.
    //
    // Memory model of QEMU: docs/devel/memory.rst
    //
    // - memory_region_init_io
    // - memory_region_init_rom_device_nomigrate
    // - memory_region_init_rom_device
    if (object_dynamic_cast(child, TYPE_MEMORY_REGION))
    {
        MemoryRegion *memory = MEMORY_REGION(child);
        if (memory->ops && (memory->ops->read || memory->ops->write || memory->ops->read_with_attrs || memory->ops->write_with_attrs))
        {
            extern const MemoryRegionOps unassigned_mem_ops;
            if (memory->ops == &unassigned_mem_ops)
            {
                g_print("unassigned_mem_ops\n");
            }

            g_print("MemoryRegion(%s) { 0x%.16llx | 0x%.16llx }\n",
                    memory_region_name(memory),
                    memory->addr,
                    int128_getlo(memory_region_size(memory)));

            g_print("  Handled by MemoryRegionOps(%p)\n", memory->ops);

            if (memory->ops->read)
                g_print("    read\n");

            if (memory->ops->write)
                g_print("    write\n");

            if (memory->ops->read_with_attrs)
                g_print("    read_with_attrs\n");

            if (memory->ops->write_with_attrs)
                g_print("    write_with_attrs\n");

            // TODO: How to check if the memory is MMIO or regular?
            assert(memory->enabled);
        }

        return 0;
    }

    // g_print("Object = %s\n", object_get_typename(child));

    // type_name = apic
    // type_name = container
    // type_name = e1000
    // type_name = floppy
    // type_name = floppy-bus
    // type_name = fw_cfg_io
    // type_name = hpet
    // type_name = i2c-bus
    // type_name = i440FX
    // type_name = i440FX-pcihost
    // type_name = i8042
    // type_name = i8257
    // type_name = IDE
    // type_name = ide-cd
    // type_name = ioapic
    // type_name = irq
    // type_name = ISA
    // type_name = isa-fdc
    // type_name = isa-i8259
    // type_name = isa-parallel
    // type_name = isa-pcspk
    // type_name = isa-pit
    // type_name = isa-serial
    // type_name = kvmvapic
    // type_name = mc146818rtc
    // type_name = memory-region
    // type_name = pc-i440fx-6.2-machine
    // type_name = PCI
    // type_name = PIIX3
    // type_name = piix3-ide
    // type_name = PIIX4_PM
    // type_name = port92
    // type_name = qemu64-x86_64-cpu
    // type_name = qtest
    // type_name = serial
    // type_name = smbus-eeprom
    // type_name = System
    // type_name = VGA
    // type_name = vmmouse
    // type_name = vmport

    return 0;
}

static void list_fuzzable(void)
{
    Object *root = qdev_get_machine();

    g_print("type_name = %s\n", object_get_typename(root));

    object_child_foreach_recursive(root,
                                   list_fuzzable_objects,
                                   NULL);
}

static const char *fuzz_arch = "x86_64";
static QTestState *fuzz_qts = NULL;

static int garbage_test(void)
{
    if (!strcmp(fuzz_arch, "false"))
    {
        qemu_mutex_lock_iothread();

        if (!strcmp(fuzz_arch, "false"))
        {
            goto err;
        }

        qemu_mutex_unlock_iothread();
    }

err:
    return -1;
}

int main(int argc, char **argv)
{
    garbage_test();

    printf("Starting goose\n");

    // TODO: This graph thing is used in a way I don't understand.
    qos_graph_init();

    // Initialize the QEMU Object Model.
    module_call_init(MODULE_INIT_QOM);

    // TODO: This creates some QOS nodes that seem to make qemu load some drivers.
    // We need to know what this does and if it affects our research.
    module_call_init(MODULE_INIT_LIBQOS);

    qemu_init_exec_dir(*argv);

    // Sets the server's tx to in process.
    qtest_server_set_send_handler(&qtest_client_inproc_recv, &fuzz_qts);

    // Sets the client's rx and tx to in process. This means we don't use the socket.
    fuzz_qts = qtest_inproc_init(&fuzz_qts, false, fuzz_arch,
                                 &qtest_server_inproc_recv);

    // Prepare a command line.
    GString *cmd_line = g_string_new(TARGET_NAME);
    g_string_append_printf(cmd_line, " -display none");
    g_string_append_printf(cmd_line, " -machine accel=qtest");
    g_string_append_printf(cmd_line, " -m 512M");
    g_string_append_printf(cmd_line, " -qtest-log /dev/stdout");
    g_string_append_printf(cmd_line, " -qtest /dev/null");
    g_string_append_printf(cmd_line, " -smp 2");
    g_string_append_printf(cmd_line, " -nic user,model=virtio-net-pci");
    // Test some variants of parallel port
    // g_string_append_printf(cmd_line, " -chardev serial,id=ser0,path=/dev/null");
    // g_string_append_printf(cmd_line, " -device isa-parallel,chardev=ser0,index=0,iobase=0x378,irq=7");
    // g_string_append_printf(cmd_line, " -chardev file,id=parallel0,path=/dev/null");
    // g_string_append_printf(cmd_line, " -device isa-parallel,chardev=parallel0,index=0,iobase=0x378,irq=7");

    // Split the command line.
    wordexp_t result;
    wordexp(cmd_line->str, &result, 0);
    g_string_free(cmd_line, true);

    // Run QEMU's softmmu main with the fuzz-target dependent arguments.
    qemu_init(result.we_wordc, result.we_wordv, NULL);

    // TODO: What is this?.
    rcu_enable_atfork();

    signal(SIGINT, SIG_DFL);
    signal(SIGHUP, SIG_DFL);
    signal(SIGTERM, SIG_DFL);

    // Get a list of everything we can touch.
    list_fuzzable();

    printf("pre\n");
    qtest_outb(fuzz_qts, 0x70, 0xca + 1);
    printf("pst\n");

    qtest_outb(fuzz_qts, 0x3f8, 1);
    qtest_outw(fuzz_qts, 0x3f8, 2);
    qtest_outl(fuzz_qts, 0x3f8, 4);

    // Trigger something.
    qtest_writeb(fuzz_qts, 0x378, 1);
    qtest_writew(fuzz_qts, 0x378, 2);
    qtest_writel(fuzz_qts, 0x378, 4);
    qtest_writeq(fuzz_qts, 0x378, 8);

    // Run qemu's event loop for a while.
#define MAX_EVENT_LOOPS 10
    int i = MAX_EVENT_LOOPS;
    while (g_main_context_pending(NULL) && i-- > 0)
    {
        printf("Loop\n");
        main_loop_wait(false);
    }

    return 0;
}
