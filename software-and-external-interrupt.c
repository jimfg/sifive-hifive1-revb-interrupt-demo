/* This program is similar to nested-external-interrupt.c
 * but tries to make software intr (which has lower priority than 
 * external intr, fixed by CLINT) be taken while handling external intr.
 *
 * in external intr handling make a sip pending, see whether it 
 * jumps to soft intr handling.
 */
#include <stdio.h>
#include <metal/machine.h>
#include <metal/machine/platform.h>

struct metal_cpu *cpu;
struct metal_interrupt *cpu_intr;
struct metal_interrupt *plic;
struct metal_interrupt *clint;

void my_plic0_handler(int id, void *priv)
{
	int rc;
	printf("my_plic0_handler\r\n");
	
	volatile unsigned *addr = (unsigned *)0xc200004U;
	unsigned val;
	/* claim, the corresponding PLIC IP and PLIC eip will be cleared */
	val = *addr;

	unsigned i = 8;
	printf("enable mstatus.MIE again\r\n");
	__asm__ volatile("csrs mstatus, %0" : "=r"(i));
	
	printf("pend self a software intr\r\n");
	volatile unsigned *clint_msip = (unsigned *)0x2000000U;
	*clint_msip = 1;

	printf("here?\r\n");

	/* complete */
	*addr = val;
}

void my_soft_handler(int id, void *priv)
{
	/* write to 0x2000000 to clear mip.MSIP, but not do so
	 * since soft intr nested in extern intr is already seen
	 */
	static int i;
	printf(i++ % 60000 == 0 ? "!\r\n" : "");
}

int main(void)
{
	struct metal_pwm *pwm1;
	int pwm1_id0, rc;
	
	int i = 0;
	__asm__ volatile("csrw mcycle, %0" : "=r"(i));
	i = 8;
	__asm__ volatile("csrc mip, %0" : "=r"(i));

	cpu = metal_cpu_get(metal_cpu_get_current_hartid());
	if (cpu == NULL)
		return 1;
	cpu_intr = metal_cpu_interrupt_controller(cpu);
	if (cpu == NULL)
		return 1;
	metal_interrupt_init(cpu_intr);

	plic = metal_interrupt_get_controller(METAL_PLIC_CONTROLLER, 0);
	if (plic == NULL)
		return 1;
	metal_interrupt_init(plic);

	/* initialize CLINT, enable software intr */
	clint = metal_interrupt_get_controller(METAL_CLINT_CONTROLLER, 0);
	if (clint == NULL)
		return 1;
	/* for line 3, 7, register it in cpu_intr, set default func, not enable */
	metal_interrupt_init(clint);
	/* enable software intr */
	if (metal_interrupt_enable(clint, 3))
		return 1;
	/* change isr */
	if (metal_interrupt_register_handler(cpu_intr, 3, my_soft_handler, clint))
		return 1;
	
	pwm1 = metal_pwm_get_device(1);
	if (pwm1 == NULL)
		return 1;
	pwm1_id0 = metal_pwm_get_interrupt_id(pwm1, 0);
	
	metal_interrupt_set_priority(plic, pwm1_id0, 2);
	
	cpu_intr = metal_cpu_interrupt_controller(cpu);
	if (cpu_intr == NULL)
		return 1;
	rc = metal_interrupt_register_handler(cpu_intr, 11, my_plic0_handler, plic);
	if (rc)
		return 1;

	metal_pwm_enable(pwm1);
	metal_pwm_set_freq(pwm1, 0, 1);
	metal_pwm_set_duty(pwm1, 1, 0, METAL_PWM_PHASE_CORRECT_DISABLE);
	metal_pwm_set_duty(pwm1, 2, 0, METAL_PWM_PHASE_CORRECT_DISABLE);
	metal_pwm_set_duty(pwm1, 3, 0, METAL_PWM_PHASE_CORRECT_DISABLE);

	metal_pwm_trigger(pwm1, 0, METAL_PWM_CONTINUOUS);
	metal_pwm_cfg_interrupt(pwm1, METAL_PWM_INTERRUPT_ENABLE);

	if (metal_interrupt_enable(cpu_intr, 0))
		return 1;
	if (metal_interrupt_enable(plic, pwm1_id0))
		return 1;
	
	while (1) ;
	return 2;
}
