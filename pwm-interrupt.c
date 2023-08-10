/* This program prints the value in mcycle once/second.
 * It is for getting familiar with pwm peripheral and PLIC basis.
 *
 * pwmcmp0ip signals an interrupt to PLIC gateway every 1 second.
 * the handler will
 * [claim] corresponding IP bit in PLIC is cleared, intr id returned.
 * [clear pwmcmp0ip] pending flag at the intr source need to be cleared.
 * [complete] so PLIC gateway can send intr request again.
 */
#include <stdio.h>
#include <metal/machine.h>
#include <metal/machine/platform.h>

struct metal_cpu *cpu;

/* param will be given: id in PLIC, 44, the pwm structure */
void pwm1_isr0(int pwm_id, void *data)
{
	/* clear pwmcmp0ip */
	volatile unsigned *addr = (unsigned *)0x10025000U;
	unsigned val;
	val = *addr;
	val &= ~0x10000000;
	*addr = val;
	
	int i;
	__asm__ volatile("csrr %0, mcycle" : "=r"(i));
	printf("mcycle=0x%x\r\n", i);
}

int main(void)
{
	struct metal_interrupt *cpu_intr;
	struct metal_interrupt *plic;
	struct metal_pwm *pwm1;
	int pwm1_id0, rc;
	
	int i = 0;
	__asm__ volatile("csrw mcycle, %0" :: "r"(i));

	/* initialize cpu interrupt controller */
	cpu = metal_cpu_get(metal_cpu_get_current_hartid());
	if (cpu == NULL)
		return 1;
	cpu_intr = metal_cpu_interrupt_controller(cpu);
	if (cpu == NULL)
		return 1;
	/* set mtvec __metal_exception_handler
	 * initialize table entries in cpu_intr structure
	 */
	metal_interrupt_init(cpu_intr);

	/* initialize PLIC */
	plic = metal_interrupt_get_controller(METAL_PLIC_CONTROLLER, 0);
	if (plic == NULL)
		return 1;
	/* initialize PLIC regs, and the entries in plic structure
	 * save __metal_plic0_handler in int_table[11] in cpu_intr,
	 * the handler will claim and complete. set mie.MEIE 1. 
	 */
	metal_interrupt_init(plic);

	/* pwm and its source for PLIC */
	pwm1 = metal_pwm_get_device(1);
	if (pwm1 == NULL)
		return 1;
	/* will get 44 in PLIC lines */
	pwm1_id0 = metal_pwm_get_interrupt_id(pwm1, 0);
	metal_interrupt_set_priority(plic, pwm1_id0, 2);
	/* save isr in PLIC table entry */
	rc = metal_interrupt_register_handler(plic, pwm1_id0, pwm1_isr0, pwm1);
	if (rc)
		return 1;

	/* set and select iof for gpio 19-22 */
	metal_pwm_enable(pwm1);
	/* set pwmscale, pwmcmp0 to match freq 1Hz */
	metal_pwm_set_freq(pwm1, 0, 1);
	/* set pwmcmp1-3 to have pwmcmpxgpio always high */
	metal_pwm_set_duty(pwm1, 1, 0, METAL_PWM_PHASE_CORRECT_DISABLE);
	metal_pwm_set_duty(pwm1, 2, 0, METAL_PWM_PHASE_CORRECT_DISABLE);
	metal_pwm_set_duty(pwm1, 3, 0, METAL_PWM_PHASE_CORRECT_DISABLE);

	/* set pwmdeglitch, pwmzerocmp, pwmenalways */
	metal_pwm_trigger(pwm1, 0, METAL_PWM_CONTINUOUS);
	/* set pwmsticky */
	metal_pwm_cfg_interrupt(pwm1, METAL_PWM_INTERRUPT_ENABLE);

	/* set mstatus.MIE */
	if (metal_interrupt_enable(cpu_intr, 0))
		return 1;
	/* in PLIC enable the source corresponding to pwmcmp0ip */
	if (metal_interrupt_enable(plic, pwm1_id0))
		return 1;
	
	while (1) ;
	return 2;
}
