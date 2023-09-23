# sifive-hifive1-revb-interrupt-demo
These demos use newlib C and Freedom Metal library: https://github.com/sifive/freedom-metal/tree/master  
  
pwm-interrupt.c  
  pwm interrupt through PLIC once/second  
  
nested-external-interrupt.c  
  in first pwm intr taken, set mstatus.MIE before claim  
  
software-and-external-interrupt.c  
  in first pwm intr taken, pend self a software intr  
    
plic-gateway-request-1.c  
  written due to misunderstanding of PLIC spec, just keep it
  
nested-plic-interrupt.c  
  make nested interrupts come from different sources to PLIC  
  
intr-code-mcycle.c  
  estimate number of machine cycles spent with intr/non-intr code  
