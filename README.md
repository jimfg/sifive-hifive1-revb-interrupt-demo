# sifive-hifive1-revb-interrupt-demo
to demonstrate some interrupt related behaviors. 

pwm-interrupt.c 
  pwm interrupt through PLIC once/second 
 
nested-external-interrupt.c 
  in first pwm intr taken, set mstatus.MIE before claim 
 
software-and-external-interrupt.c 
  in first pwm intr taken, pend self a software intr
