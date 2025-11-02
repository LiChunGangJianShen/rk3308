#include <stdio.h>
#include <stddef.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

void check_cpu_serial_num(char *buff, int size)
{
#define SHELL_CMMD  "grep Serial /proc/cpuinfo | awk '{print $NF}'"
	FILE *fp = NULL;

	fp = popen(SHELL_CMMD, "r");
	if(NULL == fp){
		perror("popen error\n");
	}
	fread(buff, size, 1, fp);
	
	//因为grep会添加\n到行末，故需要去掉\n
	char *p = buff;
	char *p1 = strstr(buff, "\n");
	buff[p1 - p] = '\0';

	pclose(fp);
}

#ifdef __cplusplus
}
#endif