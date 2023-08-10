# sifive-hifive1-revb-interrupt-demo
These demos use newlib C and Freedom Metal library: https://github.com/sifive/freedom-metal/tree/master  
  
pwm-interrupt.c  
  pwm interrupt through PLIC once/second  
  
nested-external-interrupt.c  
  in first pwm intr taken, set mstatus.MIE before claim  
  
software-and-external-interrupt.c  
  in first pwm intr taken, pend self a software intr  
  
nested-plic-interrupt.c  
  make nested interrupts come from different sources to PLIC  
