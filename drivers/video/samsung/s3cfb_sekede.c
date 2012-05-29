#include "s3cfb.h"

extern struct s3cfb_lcd sekede_lcd;
/* name should be fixed as 's3cfb_set_lcd_info' */
void s3cfb_set_lcd_info(struct s3cfb_global *ctrl)
{
	sekede_lcd.init_ldi = NULL;
	ctrl->lcd = &sekede_lcd;
}

