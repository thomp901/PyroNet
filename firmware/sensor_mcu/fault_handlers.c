#include "fault_handlers.h"

#include <stddef.h>
#include <stdint.h>

#include "debug_console.h"
#include "em_device.h"

typedef struct {
  uint32_t r0;
  uint32_t r1;
  uint32_t r2;
  uint32_t r3;
  uint32_t r12;
  uint32_t lr;
  uint32_t pc;
  uint32_t xpsr;
} fault_stack_frame_t;

static void fault_write_hex32(uint32_t value)
{
  static const char digits[] = "0123456789ABCDEF";
  char text[] = "0x00000000";
  unsigned int index;

  for (index = 0U; index < 8U; index++) {
    text[2U + index] = digits[(value >> ((7U - index) * 4U)) & 0xFU];
  }

  debug_console_panic_write(text);
}

static void fault_write_field(const char *name, uint32_t value)
{
  debug_console_panic_write(" ");
  debug_console_panic_write(name);
  debug_console_panic_write("=");
  fault_write_hex32(value);
}

static void fault_report_stack(const fault_stack_frame_t *stack)
{
  if (stack == NULL) {
    debug_console_panic_write("FAULT_STACK unavailable\n");
    return;
  }

  debug_console_panic_write("FAULT_STACK");
  fault_write_field("r0", stack->r0);
  fault_write_field("r1", stack->r1);
  fault_write_field("r2", stack->r2);
  fault_write_field("r3", stack->r3);
  debug_console_panic_write("\n");

  debug_console_panic_write("FAULT_STACK");
  fault_write_field("r12", stack->r12);
  fault_write_field("lr", stack->lr);
  fault_write_field("pc", stack->pc);
  fault_write_field("xpsr", stack->xpsr);
  debug_console_panic_write("\n");
}

static void fault_report_scb(void)
{
  debug_console_panic_write("FAULT_SCB");
  fault_write_field("cfsr", SCB->CFSR);
  fault_write_field("hfsr", SCB->HFSR);
  fault_write_field("dfsr", SCB->DFSR);
  fault_write_field("afsr", SCB->AFSR);
  debug_console_panic_write("\n");

  debug_console_panic_write("FAULT_SCB");
  fault_write_field("mmfar", SCB->MMFAR);
  fault_write_field("bfar", SCB->BFAR);
  fault_write_field("shcsr", SCB->SHCSR);
  debug_console_panic_write("\n");
}

__attribute__((noreturn)) static void fault_report(
  const char *type,
  const fault_stack_frame_t *stack,
  uint32_t exc_return)
{
  __disable_irq();

  debug_console_panic_write("\nFAULT type=");
  debug_console_panic_write(type);
  fault_write_field("exc_return", exc_return);
  fault_write_field("ipsr", __get_IPSR());
  fault_write_field("sp", (uint32_t)stack);
  debug_console_panic_write("\n");

  fault_report_stack(stack);
  fault_report_scb();
  debug_console_panic_write("FAULT_HALT\n");

  while (1) {
    __NOP();
  }
}

void fault_handlers_init(void)
{
  SCB->SHCSR |= SCB_SHCSR_MEMFAULTENA_Msk
                | SCB_SHCSR_BUSFAULTENA_Msk
                | SCB_SHCSR_USGFAULTENA_Msk;
#ifdef SCB_SHCSR_SECUREFAULTENA_Msk
  SCB->SHCSR |= SCB_SHCSR_SECUREFAULTENA_Msk;
#endif
}

__attribute__((naked)) void HardFault_Handler(void)
{
  __asm volatile(
    "tst lr, #4\n"
    "ite eq\n"
    "mrseq r0, msp\n"
    "mrsne r0, psp\n"
    "mov r1, lr\n"
    "b fault_hard_fault_c\n"
  );
}

__attribute__((noreturn)) void fault_hard_fault_c(uint32_t *stack,
                                                  uint32_t exc_return)
{
  fault_report("HardFault", (const fault_stack_frame_t *)stack, exc_return);
}

__attribute__((naked)) void MemManage_Handler(void)
{
  __asm volatile(
    "tst lr, #4\n"
    "ite eq\n"
    "mrseq r0, msp\n"
    "mrsne r0, psp\n"
    "mov r1, lr\n"
    "b fault_mem_manage_c\n"
  );
}

__attribute__((noreturn)) void fault_mem_manage_c(uint32_t *stack,
                                                  uint32_t exc_return)
{
  fault_report("MemManage", (const fault_stack_frame_t *)stack, exc_return);
}

__attribute__((naked)) void BusFault_Handler(void)
{
  __asm volatile(
    "tst lr, #4\n"
    "ite eq\n"
    "mrseq r0, msp\n"
    "mrsne r0, psp\n"
    "mov r1, lr\n"
    "b fault_bus_fault_c\n"
  );
}

__attribute__((noreturn)) void fault_bus_fault_c(uint32_t *stack,
                                                 uint32_t exc_return)
{
  fault_report("BusFault", (const fault_stack_frame_t *)stack, exc_return);
}

__attribute__((naked)) void UsageFault_Handler(void)
{
  __asm volatile(
    "tst lr, #4\n"
    "ite eq\n"
    "mrseq r0, msp\n"
    "mrsne r0, psp\n"
    "mov r1, lr\n"
    "b fault_usage_fault_c\n"
  );
}

__attribute__((noreturn)) void fault_usage_fault_c(uint32_t *stack,
                                                   uint32_t exc_return)
{
  fault_report("UsageFault", (const fault_stack_frame_t *)stack, exc_return);
}

__attribute__((naked)) void SecureFault_Handler(void)
{
  __asm volatile(
    "tst lr, #4\n"
    "ite eq\n"
    "mrseq r0, msp\n"
    "mrsne r0, psp\n"
    "mov r1, lr\n"
    "b fault_secure_fault_c\n"
  );
}

__attribute__((noreturn)) void fault_secure_fault_c(uint32_t *stack,
                                                    uint32_t exc_return)
{
  fault_report("SecureFault", (const fault_stack_frame_t *)stack, exc_return);
}

__attribute__((naked)) void NMI_Handler(void)
{
  __asm volatile(
    "tst lr, #4\n"
    "ite eq\n"
    "mrseq r0, msp\n"
    "mrsne r0, psp\n"
    "mov r1, lr\n"
    "b fault_nmi_c\n"
  );
}

__attribute__((noreturn)) void fault_nmi_c(uint32_t *stack,
                                           uint32_t exc_return)
{
  fault_report("NMI", (const fault_stack_frame_t *)stack, exc_return);
}
