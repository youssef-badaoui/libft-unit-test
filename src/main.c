/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.c                                             :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: alelievr <alelievr@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2015/11/13 19:59:29 by alelievr          #+#    #+#             */
/*   Updated: 2016/08/06 18:34:27 by alelievr         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <dlfcn.h>
#include <time.h>
#include <execinfo.h>
#include <locale.h>
#include <sys/mman.h>
#include "libft_test.h"
#include "init.c"
#include <errno.h>
#include <time.h>
#include <sys/time.h>
#include <stdarg.h>

int		fd_pipe[2];
int		_stdout;

static t_option options[] = {
	{"-versus", 'v', &g_versus},
	{"-bench", 'b', &g_bench},
	{"-nospeed", 'n', &g_nospeed},
	{NULL, 0, NULL}
};

unsigned long long	ft_clock(void) {
	return (clock());
}

void	fd_to_buffer(int fd) {
	_stdout = dup(fd);
	if (pipe(fd_pipe) != 0)
		return ;
	dup2(fd_pipe[1], fd);
	close(fd_pipe[1]);
}

int		get_last_malloc_size(void) {
	return ((int)OFF_ALLOC_SIZE);
}

char	*get_fd_buffer(int fd, char *buff, size_t size) {
	int		ret;

	if (buff == NULL) {
		char	b[0xF000];
		read(fd_pipe[0], b, sizeof(b));
		dup2(_stdout, fd);
		return (NULL);
	}
	ret = read(fd_pipe[0], buff, size);
	buff[ret] = 0;
	dup2(_stdout, fd);
	return (buff);
}

void	ft_exit(char *str, ...) {
	va_list v;
	va_start(v, str);
	vprintf(str, v);
	va_end(v);
	exit(-1);
}

void	ft_raise(int s) {
	display_test_result(s, current_explication);
}

void	load_test(void) {
	for (int i = 0; fun_test_table[i].fun_name; i++) {
		current_test_id++;
		current_fun_name = fun_test_table[i].fun_name;
		current_fun_visibility = fun_test_table[i].visible;
		fun_test_table[i].fun_test_ptr();
	}
}

void	load_bench(void)
{
	for (int i = 0; fun_bench_table[i].fun_name; i++) {
		current_test_id++;
		current_fun_name = fun_bench_table[i].fun_name;
		current_fun_visibility = fun_bench_table[i].visible;
		fun_bench_table[i].fun_test_ptr();
	}
}

void	run_subtests(void *h, int start) {
	void		*tmpfun;
	static void	*handle = NULL;

	if (!handle)
		handle = h;
	for (int i = start; fun_subtest_table[i].fun_name; i++, current_subtest_id++)
	{
		MALLOC_RESET;
		RESET_DIFF;
		SET_CURRENT_PROTECTED(INVISIBLE);
		current_fun_name = fun_subtest_table[i].fun_name;
		tmpfun = dlsym(handle, fun_subtest_table[i].fun_name);
		if (tmpfun)
			fun_subtest_table[i].fun_test_ptr(tmpfun);
		else if (fun_subtest_table[i].visible)
			ft_raise(TEST_MISSING);
	}
	current_fun_name = "";
	display_test_result(TEST_FINISHED, "");
	printf("\nSee %s for more informations !\n", LOG_FILE);
	dprintf(g_log_fd, "\n");
	close(g_log_fd);
	exit(0);
}

void	run_subbench(void *h, void *h2)
{
	void		*tmpfun;
	void		*vsfun = NULL;

	for (int i = 0; fun_subbench_table[i].fun_name; i++, current_subtest_id++)
	{
		RESET_DIFF;
		SET_CURRENT_PROTECTED(INVISIBLE);
		current_fun_name = fun_subbench_table[i].fun_name;
		tmpfun = dlsym(h, fun_subbench_table[i].fun_name);

		if (h2)
			vsfun = dlsym(h2, fun_subbench_table[i].fun_name);

		vsfun = NULL;
		if (tmpfun)
			fun_subbench_table[i].fun_bench_ptr(tmpfun, vsfun);
		else
			ft_raise(TEST_MISSING);
	}
	current_fun_name = "";
	display_test_result(TEST_FINISHED, "");
	printf("\nSee %s for more informations !\n", LOG_FILE);
	dprintf(g_log_fd, "\n");
	close(g_log_fd);
	exit(0);
}

void	*timer(void *t) {
	static int	time = 0;
	static int	last_test_id = 0;

	int			timeout_millis = TIMEOUT_MILLIS;
	if (g_bench || (g_versus != NULL && g_versus != (char *)0x1))
		timeout_millis *= 10;
	(void)t;
	while (42) {
		if (last_test_id != current_subtest_id)
			time = 0;
		if (time >= timeout_millis) {
			time = 0;
			g_time.state = TEST_CRASH;
			kill(g_pid, SIGKILL);
		}
		time++;
		last_test_id = current_subtest_id;
		usleep(1000);
	}
	return (NULL);
}

void	load_timer(void) {
	pthread_t	p;

	if ((pthread_create(&p, NULL, timer, NULL) == -1))
		ft_exit("thread inits failed !");
}

void	get_options(char **av)
{
	int		i = 0, j;

	while (av[++i])
	{
		j = -1;
		while (options[++j].long_name)
		{
			if (!strcmp(options[j].long_name, av[i]) ||
					!strcmp((char *)(char[3]){'-', options[j].short_name, 0}, av[i]))
			{
				if (av[i + 1] && options[j].arg)
					*(unsigned long *)options[j].arg = (unsigned long)av[i + 1];
				else if (options[j].arg)
					*(int *)options[j].arg = 1;
			}
		}
	}
}

int		main(unused int ac, char **av) {
	void	*handle = NULL;
	void	*handle_vs = NULL;

	get_options(av);
	setlocale(LC_ALL, "");
	if ((g_shared_mem = mmap(NULL, 0xF00, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, -1, 0)) == MAP_FAILED)
		puts("failed to create shared memory map !"), raise(SIGKILL);
	int	fd;
	if ((fd = open(SHARED_MEM_FILE, O_WRONLY | O_TRUNC | O_CREAT, 0600)) == -1)
		ft_exit("can't open/create shared memory file !\n");
	write(fd, &g_shared_mem, 8);
//	printf("shared map addr = %p\n", g_shared_mem);
	close(fd);
	if ((g_log_fd = open(LOG_FILE, O_WRONLY | O_TRUNC | O_CREAT, 0600)) == -1)
		ft_exit("can't open/create logfile !\n");
	if ((g_diff_fd = open(DIFF_FILE, O_RDWR | O_TRUNC | O_CREAT, 0600)) == -1)
		ft_exit("can't create/open diff file !\n");
	MALLOC_RESET;
	RESET_DIFF;
	if (!(handle = dlopen("./libft.so", RTLD_LAZY)))
		ft_exit(dlerror());

	/* Ignore user interupt signals: */
//	signal(SIGINT, SIG_IGN);
//	signal(SIGQUIT, SIG_IGN);

	/* Threads for timeout */
	load_timer();

	/* Running test for evry function: */
	load_test();
	load_bench();
	if (g_bench == 0 && g_versus == NULL)
		run_subtests(handle, 0);
	else
	{
		if (g_versus == (char *)0x1)
			ft_exit("versus: bad argument, please enter a shared library file\n");
		if (g_versus != NULL)
			if (!(handle_vs = dlopen(g_versus, RTLD_LAZY)))
				ft_exit("failed to load [%s] shared library\n", g_versus);
		run_subbench(handle, handle_vs);
	}
	munmap(g_shared_mem, 0xF00);
	return (0);
}
