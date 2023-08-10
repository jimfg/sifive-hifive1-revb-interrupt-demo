/* This program demonstrates whether mstatus.MIE=mie.MEIE=mip.MEIP=1
 * causes an external interrupt being taken while handling external interrupt.
 * Purpose is to see whether nested same level(in CLINT's view) interrupts can occur.
 *
 * change default plic handler in cpu intr controller table to my_plic0_handler,
 * this handler doesn't claim, it
 * [change plic handler to my_alt_plic0_hander] if nested intr occurs
 * 	the new handler will print different message.
 * [set mstatus.MIE] so mstatus.MIE=mie.MEIE=mip.MEIP=1
 */
#include <stdio.h>
#include <metal/machine.h>
#include <metal/machine/platform.h>

struct metal_cpu *cpu;
struct metal_interrupt *cpu_intr;
struct metal_interrupt *plic;
void my_alt_plic0_handler(int id, void *priv);

void my_plic0_handler(int id, void *priv)
{
	int rc;
	printf("my_plic0_handler\r\n");
	
	rc = metal_interrupt_register_handler(cpu_intr, 11, my_alt_plic0_handler, plic);
	if (rc)
		printf("rc!=0\r\n");

	unsigned i = 8;
	__asm__ volatile("csrs mstatus, %0" :: "r"(i));
	/* after execute a few instructions turns to handle nested intr.
	 * this will not be printed out for a long time.
	 * only when debugging do i see it come out.
	 */
	printf("reach here?\r\n");

	/* if nested intr won't happen, change back the handler, claim, complete.
	 * it should clear pwmcmp0ip, but since happening of nested intr is already
	 * seen, the following doesn't matter much
	 */
	rc = metal_interrupt_register_handler(cpu_intr, 11, my_plic0_handler, plic);
	if (rc)
		printf("rc!=0\r\n");
	volatile unsigned *addr = (unsigned *)0xc200004U;
	printf("claim & complete\r\n");
	unsigned val;
	/* claim */
	val = *addr;
	/* complete */
	*addr = val;
}
void my_alt_plic0_handler(int id, void *priv)
{
	static int i = 0;
	printf(i++ % 60000 == 0 ? "!\r\n": "");
}

int main(void)
{
	struct metal_pwm *pwm1;
	int pwm1_id0, rc;
	
	int i = 0;
	__asm__ volatile("csrw mcycle, %0" :: "r"(i));

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

	/* change default __metal_plic0_handler to my_plic0_handler
	 * this one doesn't claim before set mstatus.MIE
	 */
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
