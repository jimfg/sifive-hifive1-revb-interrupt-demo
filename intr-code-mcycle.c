/* This program estimates the number of machine cycles occupied by 
 * code run caused by interrupt and other code.
 *
 * Want to see how much time the routines for interrupt dispatch in 
 * metal bsp spend compared to some user code maybe doesn't use bsp APIs.
 * (With some higher interrupt rates when testing FreeRTOS using 
 * IntQueue tasks I saw most of the time is used for interrupt thing. 
 * So want to see some quantitative result with simpler demo first.)
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
}

int main(void)
{
	struct metal_cpu *cpu;
	struct metal_interrupt *cpu_intr;
	struct metal_interrupt *plic;
	struct metal_pwm *pwm1;
	int pwm1_id0, rc;
	
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

	pwm1 = metal_pwm_get_device(1);
	if (pwm1 == NULL)
		return 1;
	pwm1_id0 = metal_pwm_get_interrupt_id(pwm1, 0);
	metal_interrupt_set_priority(plic, pwm1_id0, 2);
	rc = metal_interrupt_register_handler(plic, pwm1_id0, pwm1_isr0, pwm1);
	if (rc)
		return 1;

	metal_pwm_enable(pwm1);
	/* set different intr frequency here */
	metal_pwm_set_freq(pwm1, 0, 4000);
	metal_pwm_set_duty(pwm1, 1, 0, METAL_PWM_PHASE_CORRECT_DISABLE);
	metal_pwm_set_duty(pwm1, 2, 0, METAL_PWM_PHASE_CORRECT_DISABLE);
	metal_pwm_set_duty(pwm1, 3, 0, METAL_PWM_PHASE_CORRECT_DISABLE);

	metal_pwm_trigger(pwm1, 0, METAL_PWM_CONTINUOUS);
	metal_pwm_cfg_interrupt(pwm1, METAL_PWM_INTERRUPT_ENABLE);

	if (metal_interrupt_enable(cpu_intr, 0))
		return 1;
	if (metal_interrupt_enable(plic, pwm1_id0))
		return 1;

	int i = 0, i_saved;
	int count_intr = 0, count_non_intr = 0;
	int while_count = 1000000;
	/* it takes about 4 minutes to overflow mcycle to
	 * mcycleh. mcycle only is enough to see result.
	 */
	__asm__ volatile("csrw mcycle, %0" :: "r"(i));

	/* count mcycle for non-intr/intr code */
	__asm__ volatile("csrr %0, mcycle" : "=r"(i_saved));
	while (while_count-- > 0) {
		__asm__ volatile("csrr %0, mcycle" : "=r"(i));
		if (i - i_saved > 50) {
			count_intr += i - i_saved;
		} else {
			count_non_intr += i - i_saved;
		}
		i_saved = i;
	}
	printf("intr: %d\r\nnon_intr: %d\r\n", count_intr, count_non_intr);

	while (1) {
		;
	}
	return 2;
}
