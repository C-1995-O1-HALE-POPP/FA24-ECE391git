#include <stdint.h>

#include "timer.h"
#include "string.h" // for memset()
#include "serial.h"

#define CH0 '.'
#define CH1 '@'
#define NCOL 80

static void init(char * prev, char * next);
static void step(const char * prev, char * next);
static void draw(int comno, const char * line);
static void wait(void);

void rule30_main(int comno) {
    static char buf[2][NCOL];
    char * prev = buf[0];
    char * next = buf[1];
    char * tmp;

    init(prev, next);
    draw(comno, prev);

    for (;;) {
        step(prev, next);
        draw(comno, next);
        
        tmp = next;
        next = prev;
        prev = tmp;
        wait();
    }
}

void init(char * prev, char * next) {
    memset(prev, CH0, NCOL);
    memset(next, CH0, NCOL);
    prev[NCOL/2] = CH1;
}

void step(const char * prev, char * next) {
    static const char rule[] = {
        CH0, CH1, CH1, CH1, CH1, CH0, CH0, CH0
    };

    int idx;
    int i;

    for (i = 0; i < NCOL; i++) {
        idx = (prev[(i+1) % NCOL] != CH0);
        idx = (idx << 1) | (prev[i] != CH0);
        idx = (idx << 1) | (prev[(i-1) % NCOL] != CH0);
        next[i] = rule[idx];
    }
}

void draw(int comno, const char * line) {
    int i;

    for (i = 0; i < NCOL; i++)
        com_putc(comno, line[i]);
    com_putc(comno, '\r');
    com_putc(comno, '\n');
}

void wait(void) {
    condition_wait(&tick_10Hz);
}