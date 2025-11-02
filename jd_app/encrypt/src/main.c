#include <string.h>
#include "h20_encrypt.h"

int main(int argc, char **argv)
{
	if(!strcmp(argv[1], "program")){
		h20_gen_fa_program();
	}
	else if(!strcmp(argv[1], "test")){
		h20_gen_fa_test();
	}
	else if(!strcmp(argv[1], "eeprom_test")){
		h20_gen_fa_eeprom_test();
	}
	else if(!strcmp(argv[1], "verify")){
		h20_gen_fa_verify();
	}

	return 0;
}
