#pragma once

void pic_remap(void);
void pic_mask_all(void);
void pic_unmask(unsigned irq);
void pic_eoi(unsigned irq);
