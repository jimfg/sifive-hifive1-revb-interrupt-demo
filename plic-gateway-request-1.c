/* This program is written because of misunderstanding of the
 * interrupt flow picture in RISC-V Platform-Leven Interrupt
 * Controller Specification.
 * (I originally thought at some time all gateways will stop 
 * generate request until the completion process takes place. 
 * The spec says "The interrupt gateway can now forward another 
 * inpterrupt request for the same source to the PLIC." So turns
 * out that stopping generate request is just for THE source.)
 */

/* Interrupt source pwmcmp1ip pends, but its priority
 * doesn't exceed PLIC threshold, Then pwmcmp0ip pends, its
 * priority exceeds PLIC threshold.
 * Purpose is to show whether generating an interrupt request
 * stops PLIC gateways from generating more.
 */
#include <stdio.h>
#include <metal/machine.h>
#include <metal/machine/platform.h>

void pwm1_isr0(int pwm_id, void *data)
{
	/* clear pwmcmp0ip */
	volatile unsigned *addr = (unsigned *)0x10025000U;
	unsigned val;
	val = *addr;
	val &= ~0x10000000;
	*addr = val;

	/* if PLIC gateways stop to forward request once any
	 * request is forwarded and start again when the interrupt
	 * completion process finishes, this should never be reached
	 *
	 * (due to the timing(pwm1 is start before the corresponding
	 * intr sources in PLIC are enabled), this in fact may be 
	 * called once if the above conditions hold)
	 */	
	printf("reach here?\r\n");
}

void pwm1_isr1(int pwm_id, void *data)
{
	/* since the duty of pwm1.pwmcmp1gpio is 50(/100),
	 * clearing pwmcmp1ip doesn't work until it is low by itself.
	 * not clear it since keeping pwmcmp1ip asserted is better.
	 * 
	 * this isr shouldn't be called
	 */
	static int i = 0;
	printf(i++ %30000 == 0 ? "!\r\n" : "");
}

int main(void)
{
	struct metal_cpu *cpu;
	struct metal_interrupt *cpu_intr;
	struct metal_interrupt *plic;
	struct metal_pwm *pwm1;
	int pwm1_id0, pwm1_id1, rc;
	
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
	/* in this set priority threshold in PLIC to 0 */
	metal_interrupt_init(plic);
	/* change threshold to 1 */
	metal_interrupt_set_threshold(plic, 1);

	pwm1 = metal_pwm_get_device(1);
	if (pwm1 == NULL)
		return 1;
	pwm1_id0 = metal_pwm_get_interrupt_id(pwm1, 0);
	pwm1_id1 = metal_pwm_get_interrupt_id(pwm1, 1);
	rc = metal_interrupt_register_handler(plic, pwm1_id0, pwm1_isr0, pwm1);
	if (rc)
		return 1;
	rc = metal_interrupt_register_handler(plic, pwm1_id1, pwm1_isr1, pwm1);
	if (rc)
		return 1;
	/* source pwmcmp0ip's threshold exceeds threshold,
	 * source pwmcmp1ip's threshold doesn't.
	 * this should be after the above register function
	 * be called since the register function changes priority.
	 */
	metal_interrupt_set_priority(plic, pwm1_id0, 3);
	metal_interrupt_set_priority(plic, pwm1_id1, 0);

	metal_pwm_enable(pwm1);
	metal_pwm_set_freq(pwm1, 0, 1);
	/* set pwmcmp1gpio's duty to 50 so pwmcmp1ip will be pending
	 * earlier than pwmcmp0ip
	 */
	metal_pwm_set_duty(pwm1, 1, 50, METAL_PWM_PHASE_CORRECT_DISABLE);
	metal_pwm_set_duty(pwm1, 2, 0, METAL_PWM_PHASE_CORRECT_DISABLE);
	metal_pwm_set_duty(pwm1, 3, 0, METAL_PWM_PHASE_CORRECT_DISABLE);

	metal_pwm_trigger(pwm1, 0, METAL_PWM_CONTINUOUS);
	metal_pwm_cfg_interrupt(pwm1, METAL_PWM_INTERRUPT_ENABLE);

	if (metal_interrupt_enable(cpu_intr, 0))
		return 1;
	if (metal_interrupt_enable(plic, pwm1_id0))
		return 1;
	if (metal_interrupt_enable(plic, pwm1_id1))
		return 1;
	
	while (1) ;
	return 2;
}
