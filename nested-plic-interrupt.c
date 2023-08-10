/* This program makes two interrupts caused by different sources to
 * PLIC nested.
 *
 * pwm1.pwmcmp0ip asserts interrupt level to corresponding PLIC gateway 5 times/second.
 * pwm2.pwmcmp0ip asserts interrupt level to corresponding PLIC gateway 53 times/second.
 * The later is the higher priority source to PLIC.
 *
 * Instead of using the default external interrupt handler in the 
 * __metal_driver_riscv_cpu_intc structure's metal_int_table 
 * which is assigned during the __metal_driver_riscv_plic0 structure 
 * initialization, use new_plic_handler which supports nested interrupt
 * which is also notificated to the hart by PLIC.
 */
#include <stdio.h>
#include <metal/machine.h>
#include <metal/machine/platform.h>

int nesting_depth = 0;

void new_plic_handler(int id, void *priv)
{
	/* not accurate in the sense of nesting since
	 * only the handling process reaches here does
	 * the incrementation happen
	 */
	nesting_depth++;

	/* is it good to use these structures out of
	 * metal library, it is intended to be internal?
	 */
	struct __metal_driver_riscv_plic0 *plic = priv;	

	/* interrupt claim process to PLIC */
	volatile unsigned *plic_claim_addr = (unsigned *)0xc200004U;
	unsigned plic_source = *plic_claim_addr;
	/* to show the source # and nesting depth */
	printf("%d %d\r\n", plic_source, nesting_depth);
	
	/* get the PLIC priority assigned to this source */
	unsigned plic_source_priority =
		metal_interrupt_get_priority((struct metal_interrupt *)plic, plic_source);

	/* get current priority threshold in PLIC for later restore */
	unsigned plic_threshold =
		metal_interrupt_get_threshold((struct metal_interrupt *)plic);
	
	/* rise priority threshold in PLIC*/
	metal_interrupt_set_threshold((struct metal_interrupt *)plic, plic_source_priority);

	/* save mepc for later restore */
	unsigned mepc;
	__asm__ volatile("csrr %0, mepc" : "=r"(mepc));

	/* globally enable interrupt again */
	unsigned mstatus_MIE = 8;
	__asm__ volatile("csrs mstatus, %0" :: "r"(mstatus_MIE));

	/* find source specific isr in the table and call it*/
	void (*plic_source_isr)(int, void *) = (void(*)(int, void *))0;
	if (plic_source < 53)
		plic_source_isr = plic->metal_exint_table[plic_source];
	if (plic_source_isr)
		plic_source_isr(plic_source, plic->metal_exdata_table[plic_source].exint_data);

	/* globally disable interrupt */
	__asm__ volatile("csrc mstatus, %0" :: "r"(mstatus_MIE));
	
	/* restore mepc */
	__asm__ volatile("csrw mepc, %0" :: "r"(mepc));

	/* if not set to M mode, instruction access fault later occurs
	 * from U mode after nested intr all returns, why?
	 */
	unsigned mstatus_MPP = 0x1800;
	__asm__ volatile("csrs mstatus, %0" :: "r"(mstatus_MPP));

	/* restore priority threshold in PLIC */
	metal_interrupt_set_threshold((struct metal_interrupt *)plic, plic_threshold);
	
	/* interrupt complete process to PLIC */
	*plic_claim_addr = plic_source;

	nesting_depth--;
}

void pwmx_isr0(int pwm_id, void *data)
{
	/* clear pwmx.pwmcmp0ip */
	metal_pwm_clr_interrupt((struct metal_pwm *)data, 0);
}

int main(void)
{
	struct metal_cpu *cpu;
	struct metal_interrupt *cpu_intr;
	struct metal_interrupt *plic;
	struct metal_pwm *pwm1, *pwm2;
	int pwm1_id0, pwm2_id0;
	int rc;
	
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
	/* change default plic handler which is assigned in above function */
	rc = metal_interrupt_register_handler(cpu_intr, 11, new_plic_handler, plic);
	if (rc)
		return 1;

	pwm1 = metal_pwm_get_device(1);
	if (pwm1 == NULL)
		return 1;
	pwm1_id0 = metal_pwm_get_interrupt_id(pwm1, 0);		/* source 44 for PLIC */
	rc = metal_interrupt_register_handler(plic, pwm1_id0, pwmx_isr0, pwm1);
	if (rc)
		return 1;
	metal_interrupt_set_priority(plic, pwm1_id0, 2);

	pwm2 = metal_pwm_get_device(2);
	if (pwm2 == NULL)
		return 1;
	pwm2_id0 = metal_pwm_get_interrupt_id(pwm2, 0);		/* source 48 for PLIC */
	rc = metal_interrupt_register_handler(plic, pwm2_id0, pwmx_isr0, pwm2);
	if (rc)
		return 1;
	metal_interrupt_set_priority(plic, pwm2_id0, 5);

	metal_pwm_enable(pwm1);
	metal_pwm_set_freq(pwm1, 0, 5);
	metal_pwm_set_duty(pwm1, 1, 0, METAL_PWM_PHASE_CORRECT_DISABLE);
	metal_pwm_set_duty(pwm1, 2, 0, METAL_PWM_PHASE_CORRECT_DISABLE);
	metal_pwm_set_duty(pwm1, 3, 0, METAL_PWM_PHASE_CORRECT_DISABLE);

	metal_pwm_enable(pwm2);
	metal_pwm_set_freq(pwm2, 0, 53);
	metal_pwm_set_duty(pwm2, 1, 0, METAL_PWM_PHASE_CORRECT_DISABLE);
	metal_pwm_set_duty(pwm2, 2, 0, METAL_PWM_PHASE_CORRECT_DISABLE);
	metal_pwm_set_duty(pwm2, 3, 0, METAL_PWM_PHASE_CORRECT_DISABLE);

	metal_pwm_trigger(pwm1, 0, METAL_PWM_CONTINUOUS);
	metal_pwm_cfg_interrupt(pwm1, METAL_PWM_INTERRUPT_ENABLE);
	
	metal_pwm_trigger(pwm2, 0, METAL_PWM_CONTINUOUS);
	metal_pwm_cfg_interrupt(pwm2, METAL_PWM_INTERRUPT_ENABLE);

	rc = metal_interrupt_enable(plic, pwm1_id0);
	if (rc)
		return 1;
	rc = metal_interrupt_enable(plic, pwm2_id0);
	if (rc)
		return 1;
	rc = metal_interrupt_enable(cpu_intr, 0);
	if (rc)
		return 1;

	while (1) ;
	return 2;
}
