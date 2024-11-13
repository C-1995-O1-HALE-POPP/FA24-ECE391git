/*
 * trek.c
 *
 * Super Star Trek Classic (v1.1)
 * Retro Star Trek Game 
 * C Port Copyright (C) 1996  <Chris Nystrom>
 *
 * Reworked for Fuzix by Alan Cox (C) 2018
 *	- Removed all floating point
 *	- Fixed multiple bugs in the BASIC to C conversion
 *	- Fixed a couple of bugs in the BASIC that either got in during it's
 *	  conversion between BASICs or from the original trek
 *	- Put it on a diet to get it to run in 32K. No features were harmed
 *	  in the making of this code smaller.
 *
 * TODO:
 *	- Look hard at all the rounding cases
 *	- Review some of the funnies in the BASIC code that appear to be bugs
 *	  either in the conversion or between the original and 'super' trek
 *	  Notably need to fix the use of shield energy directly for warp
 *	- Find a crazy person to draw ascii art bits we can showfile for things
 *	  like messages from crew/docking/klingons etc
 *	- I think it would make a lot of sense to switch to real angles, but
 *	  trek game traditionalists might consider that heresy.
 *
 * 
 * This program is free software; you can redistribute it and/or modify
 * in any way that you wish. _Star Trek_ is a trademark of Paramount
 * I think.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * This is a C port of an old BASIC program: the classic Super Star Trek
 * game. It comes from the book _BASIC Computer Games_ edited by David Ahl
 * of Creative Computing fame. It was published in 1978 by Workman Publishing,
 * 1 West 39 Street, New York, New York, and the ISBN is: 0-89489-052-3.
 * 
 * See http://www.cactus.org/~nystrom/startrek.html for more info.
 *
 * Contact Author of C port at:
 *
 * Chris Nystrom
 * 1013 Prairie Dove Circle
 * Austin, Texas  78758
 *
 * E-Mail: cnystrom@gmail.com, nystrom@cactus.org
 *
 * BASIC -> Conversion Issues
 *
 *     - String Names changed from A$ to sA
 *     - Arrays changed from G(8,8) to map[9][9] so indexes can
 *       stay the same.
 *
 * Here is the original BASIC header:
 *
 * SUPER STARTREK - MAY 16, 1978 - REQUIRES 24K MEMORY
 *
 ***        **** STAR TREK ****        ****
 *** SIMULATION OF A MISSION OF THE STARSHIP ENTERPRISE,
 *** AS SEEN ON THE STAR TREK TV SHOW.
 *** ORIGINAL PROGRAM BY MIKE MAYFIELD, MODIFIED VERSION
 *** PUBLISHED IN DEC'S "101 BASIC GAMES", BY DAVE AHL.
 *** MODIFICATIONS TO THE LATTER (PLUS DEBUGGING) BY BOB
 *** LEEDOM - APRIL & DECEMBER 1974,
 *** WITH A LITTLE HELP FROM HIS FRIENDS . . .
 *** COMMENTS, EPITHETS, AND SUGGESTIONS SOLICITED --
 *** SEND TO:  R. C. LEEDOM
 ***           WESTINGHOUSE DEFENSE & ELECTRONICS SYSTEMS CNTR.
 ***           BOX 746, M.S. 338
 ***           BALTIMORE, MD  21203
 ***
 *** CONVERTED TO MICROSOFT 8 K BASIC 3/16/78 BY JOHN BORDERS
 *** LINE NUMBERS FROM VERSION STREK7 OF 1/12/75 PRESERVED AS
 *** MUCH AS POSSIBLE WHILE USING MULTIPLE STATMENTS PER LINE
 *
 */

/* Kirill Levchenko modified for ECE 391 in May 2024.
 * 
 *   1. Added compass rose.
 *   2. Added local implementation of rand() and srand().
 *   3. Removed showfile(); embed strings instead.
 *   4. Local implementations of some string functions.
 *   5. Switch to using "device" for I/O.
 */

#include <stdint.h>
#include "string.h"
#include "serial.h"

/* Standard Terminal Sizes */

#define MAXROW      24
#define MAXCOL      80

struct klingon {
  uint8_t y;
  uint8_t x;
  int16_t energy;
};

/* Function Declarations */

static void intro(void);
static void new_game(void);
static void initialize(void);
static void new_quadrant(void);
static void course_control(void);
static void complete_maneuver(uint16_t, uint16_t);
static void maneuver_energy(uint16_t);
static void short_range_scan(void);
static void long_range_scan(void);
static void phaser_control(void);
static void photon_torpedoes(void);
static void torpedo_hit(uint8_t y, uint8_t x);
static void damage_control(void);
static void shield_control(void);
static void library_computer(void);
static void galactic_record(void);
static void status_report(void);
static void torpedo_data(void);
static void nav_data(void);
static void dirdist_calc(void);
static void galaxy_map(void);
static void end_of_time(void);
static void resign_commision(void);
static void won_game(void);
static void end_of_game(void);
static void klingons_move(void);
static void klingons_shoot(void);
static void repair_damage(uint16_t warp);
static void find_set_empty_place(uint8_t t, uint8_t *z1, uint8_t *z2);
static const char *get_device_name(int n);
static void quadrant_name(uint8_t small, uint8_t y, uint8_t x);
static int distance_to(struct klingon *k);
static int square00(int16_t t);
static int cint100(int16_t d);
static void compute_vector(int16_t, int16_t, int16_t, int16_t);

/* Functions added by K. Levchenko. Definitions at end of file. */

static void rand_init(void);
static int rand(void);
static int abs(int x);
static int atoi(const char *s);
char * strcpy(char * s0, const char * s1);
char * strcat(char * s0, const char * s1);

static const char splash_txt[];
static const char instr_txt[];
static const char logo_txt[];
static const char compass_txt[];

static int exited;
static int trek_comno; // com port number to use

static void trek_putchar(char c);
static char trek_getchar(void);
static void trek_puts(const char * s);
static void trek_printf(const char * fmt, ...);
static char * trek_getsn(char * buf, size_t n);
static void trek_wait_enter(void);

extern void panic(const char * msg) __attribute__ ((noreturn)); // from halt.c

/* Global Variables */

static int8_t starbases;		/* Starbases in Quadrant */
static uint8_t base_y, base_x;		/* Starbase Location in sector */
static int8_t starbases_left;		/* Total Starbases */

static int8_t c[3][10] =	/* Movement indices 1-9 (9 is wrap of 1) */
{
  {0},
  {0, 0, -1, -1, -1, 0, 1, 1, 1, 0},
  {1, 1, 1, 0, -1, -1, -1, 0, 1, 1}
};

static uint8_t docked;			/* Docked flag */
static int energy;			/* Current Energy */
static const int energy0 = 3000;	/* Starting Energy */
static unsigned int map[9][9];		/* Galaxy. BCD of k b s plus flag */
#define MAP_VISITED 0x1000		/* Set if this sector was mapped */
static struct klingon kdata[3];		/* Klingon Data */
static uint8_t klingons;		/* Klingons in Quadrant */
static uint8_t total_klingons;		/* Klingons at start */
static uint8_t klingons_left;		/* Total Klingons left */
static uint8_t torps;			/* Photon Torpedoes left */
static const uint8_t torps0 = 10;	/* Photon Torpedo capacity */
static int quad_y, quad_x;		/* Quadrant Position of Enterprise */
static int shield;			/* Current shield value */
static uint8_t stars;			/* Stars in quadrant */
static uint16_t time_start;		/* Starting Stardate */
static uint16_t time_up;		/* End of time */
static int16_t damage[9];		/* Damage Array */
static int16_t d4;			/* Used for computing damage repair time */
static int16_t ship_y, ship_x;		/* Current Sector Position of Enterprise, fixed point */
static uint16_t stardate;		/* Current Stardate */

static uint8_t quad[8][8];
#define Q_SPACE		0
#define Q_STAR		1
#define Q_BASE		2
#define Q_KLINGON	3
#define Q_SHIP		4

static char quadname[12];		/* Quadrant name */

/* We probably need double digit for co-ordinate maths, single for time */
#define TO_FIXED(x)	((x) * 10)
#define FROM_FIXED(x)	((x) / 10)

#define TO_FIXED00(x)	((x) * 100)
#define FROM_FIXED00(x)	((x) / 100)

/*
 *	Returns an integer from 1 to spread
 */
static int get_rand(int spread)
{
  uint16_t r = rand();
  /* RAND_MAX is at least 15 bits, our largest request is for 500. The
     classic unix rand() is very poor on the low bits so swap the ends
     over */
  r = (r >> 8) | (r << 8);
  return ((r % spread) + 1);
}

/*
 *	Get a random co-ordinate
 */
static uint8_t rand8(void)
{
  return (get_rand(8));
}

static uint8_t yesno(void)
{
  char b[2];
  trek_getsn(b, sizeof(b));
  if (*b == 'y' || *b == 'Y')
    return 1;
  return 0;
}

/* Input a value between 0.00 and 9.99 */
static int16_t input_f00(void)
{
  int16_t v;
  char buf[8];
  char *x;
  trek_getsn(buf, sizeof(buf));
  x = buf;
  if (*x < '0' || '9' < *x)
    return -1;
  v = 100 * (*x++ - '0');
  if (*x == 0)
    return v;
  if (*x++ != '.')
    return -1;
  if (*x < '0' || '9' < *x)
    return -1;
  v += 10 * (*x++ - '0');
  if (!*x)
    return v;
  if (*x < '0' || '9' < *x)
    return -1;
  v += *x++ - '0';
  return v;
}

/* Integer: unsigned, or returns -1 for blank/error */
static int input_int(void)
{
  char x[40];
  trek_getsn(x, sizeof(x));
  if (*x < '0' || '9' < *x)
    return -1;
  return atoi(x);
}

static void print100(int v)
{
  const int hi = v / 100;
  const int lo = v % 100;

  if (v < 0)
    trek_printf("-");
  trek_printf("%d.", hi);
  if (lo < 10)
    trek_printf("0%d", lo);
  else
    trek_printf("%d", lo);
}

/* Main Program */

void trek_main(int comno) {
  trek_comno = comno;
  trek_wait_enter();
  rand_init();
  intro();
  new_game();
}

static int inoperable(int u) {
  if (damage[u] < 0) {
    trek_printf("%s %s inoperable.\n",
      get_device_name(u),
      u == 5 ? "are":"is");
    return 1;
  }
  return 0;
}

static void intro(void)
{
  trek_puts(splash_txt);

  trek_printf("Do you need instructions (y/N): ");

  if (yesno())
    trek_puts(instr_txt);

  trek_puts(logo_txt);

  /* Max of 4000, which works nicely with our 0.1 fixed point giving
     us a 16bit unsigned range of time */
  stardate = TO_FIXED((get_rand(20) + 20) * 100);
}

static void new_game(void)
{
  char cmd[8];

  initialize();

  new_quadrant();

  short_range_scan();

  while (!exited) {
    if (shield + energy <= 10 && (energy < 10 || damage[7] < 0)) {
      trek_puts(
        "* Fatal Error **   You've just stranded your ship in space.\n"
        "You have insufficient maneuvering energy, "
        "and Shield Control is presently\n"
        "incapable of cross circuiting to engine room!!");

      end_of_time();
    }

    trek_printf("Command? ");

    trek_getsn(cmd, sizeof(cmd));
    trek_putchar('\n');

    if (!strncmp(cmd, "nav", 3))
      course_control();
    else if (!strncmp(cmd, "srs", 3))
      short_range_scan();
    else if (!strncmp(cmd, "lrs", 3))
      long_range_scan();
    else if (!strncmp(cmd, "pha", 3))
      phaser_control();
    else if (!strncmp(cmd, "tor", 3))
      photon_torpedoes();
    else if (!strncmp(cmd, "shi", 3))
      shield_control();
    else if (!strncmp(cmd, "dam", 3))
      damage_control();
    else if (!strncmp(cmd, "com", 3))
      library_computer();
    else if (!strncmp(cmd, "xxx", 3))
      resign_commision();
    else {
      /* FIXME: showfile ?*/
      trek_puts("Enter one of the following:\n");
      trek_puts("  nav - To Set Course");
      trek_puts("  srs - Short Range Sensors");
      trek_puts("  lrs - Long Range Sensors");
      trek_puts("  pha - Phasers");
      trek_puts("  tor - Photon Torpedoes");
      trek_puts("  shi - Shield Control");
      trek_puts("  dam - Damage Control");
      trek_puts("  com - Library Computer");
      trek_puts("  xxx - Resign Command\n");
    }
  }
}

static void initialize(void)
{
  int i, j;
  static char plural_2[2] = "";
  static char plural[4] = "is";
  uint8_t yp, xp;

  /* Initialize time */

  time_start = FROM_FIXED(stardate);
  time_up = 25 + get_rand(10);

  /* Initialize Enterprise */

  docked = 0;
  energy = energy0;
  torps = torps0;
  shield = 0;

  quad_y = rand8();
  quad_x = rand8();
  ship_y = TO_FIXED00(rand8());
  ship_x = TO_FIXED00(rand8());

  for (i = 1; i <= 8; i++)
    damage[i] = 0;

  /* Setup What Exists in Galaxy */

  for (i = 1; i <= 8; i++) {
    for (j = 1; j <= 8; j++) {
      uint8_t r = get_rand(100);
      klingons = 0;
      if (r > 98)
        klingons = 3;
      else if (r > 95)
        klingons = 2;
      else if (r > 80)
        klingons = 1;

      klingons_left = klingons_left + klingons;
      starbases = 0;

      if (get_rand(100) > 96)
        starbases = 1;

      starbases_left = starbases_left + starbases;

      map[i][j] = (klingons << 8) + (starbases << 4) + rand8();
    }
  }

  /* Give more time for more Klingons */
  if (klingons_left > time_up)
    time_up = klingons_left + 1;

  /* Add a base if we don't have one */
  if (starbases_left == 0) {
    yp = rand8();
    xp = rand8();
    if (map[yp][xp] < 0x200) {
      map[yp][xp] += (1 << 8);
      klingons_left++;
    }

    map[yp][xp] += (1 << 4);
    starbases_left++;
  }

  total_klingons = klingons_left;

  if (starbases_left != 1) {
    strcpy(plural_2, "s");
    strcpy(plural, "are");
  }

  trek_printf("Your orders are as follows:\n"
         " Destroy the %d Klingon warships which have invaded\n"
         " the galaxy before they can attack Federation Headquarters\n"
         " on stardate %d. This gives you %d days. There %s\n"
         " %d starbase%s in the galaxy for resupplying your ship.\n\n"
         "Hit enter to accept command. ",
         klingons_left, time_start + time_up, time_up, plural, starbases_left, plural_2);
  trek_wait_enter();
}

static void place_ship(void)
{
  quad[FROM_FIXED00(ship_y) - 1][FROM_FIXED00(ship_x) - 1] = Q_SHIP;
}

static void new_quadrant(void)
{
  int i;
  uint16_t tmp;
  struct klingon *k = kdata;

  klingons = 0;
  starbases = 0;
  stars = 0;

  /* Random factor for damage repair. We compute it on each new
     quadrant to stop the user just retrying until they get a number
     they like. The conversion here was wrong and now copies the BASIC
     code generate 0.00 to 0.49 */
  d4 = get_rand(50) - 1;

  /* Copy to computer */
  map[quad_y][quad_x] |= MAP_VISITED;

  if (quad_y >= 1 && quad_y <= 8 && quad_x >= 1 && quad_x <= 8) {
    quadrant_name(0, quad_y, quad_x);

    if (TO_FIXED(time_start) != stardate)
      trek_printf("Now entering %s quadrant...\n\n", quadname);
    else {
      trek_puts("\nYour mission begins with your starship located");
      trek_printf("in the galactic quadrant %s.\n\n", quadname);
    }
  }

  tmp = map[quad_y][quad_x];
  klingons = (tmp >> 8) & 0x0F;
  starbases = (tmp >> 4) & 0x0F;
  stars = tmp & 0x0F;

  if (klingons > 0) {
    trek_printf("Combat Area  Condition Red\n");

    if (shield < 200)
      trek_printf("Shields Dangerously Low\n");
  }

  for (i = 1; i <= 3; i++) {
    k->y = 0;
    k->x = 0;
    k->energy = 0;
    k++;
  }

  memset(quad, Q_SPACE, 64);

  place_ship();
  
  if (klingons > 0) {
    k = kdata;
    for (i = 0; i < klingons; i++) {
      find_set_empty_place(Q_KLINGON, &k->y, &k->x);
      k->energy = 100 + get_rand(200);
      k++;
    }
  }

  if (starbases > 0)
    find_set_empty_place(Q_BASE, &base_y, &base_x);

  for (i = 1; i <= stars; i++)
    find_set_empty_place(Q_STAR, NULL, NULL);
}

static const char *inc_1 = "reports:\n  Incorrect course data, sir!\n";

static void course_control(void)
{
  register int i;
  int16_t c1;
  int16_t warp;
  uint16_t n;
  int c2, c3, c4;
  int16_t z1, z2;
  int16_t x1, x2;
  int16_t x, y;
  static char warpmax[4] = "8";

  trek_puts(compass_txt);
  trek_printf("Course (0-9): ");

  c1 = input_f00();

  if (c1 == 900)
    c1 = 100;

  if (c1 < 0 || c1 > 900) {
    trek_printf("Lt. Sulu%s", inc_1);
    return;
  }

  if (damage[1] < 0)
    strcpy(warpmax, "0.2");

  trek_printf("Warp Factor (0-%s): ", warpmax);

  warp = input_f00();

  if (damage[1] < 0 && warp > 20) {
    trek_printf("Warp Engines are damaged. "
           "Maximum speed = Warp 0.2.\n\n");
    return;
  }

  if (warp <= 0)
    return;

  if (warp > 800) {
    trek_printf("Chief Engineer Scott reports:\n"
           "  The engines won't take warp ");
    print100(warp);
    trek_puts("!\n");
    return;
  }

  n = warp * 8;

  n = cint100(n);	

  /* FIXME: should be  s + e - n > 0 iff shield control undamaged */
  if (energy - n < 0) {
    trek_printf("Engineering reports:\n"
           "  Insufficient energy available for maneuvering"
           " at warp ");
    print100(warp);
    trek_puts("!\n");

    if (shield >= n && damage[7] >= 0) {
      trek_printf("Deflector Control Room acknowledges:\n"
             "  %d units of energy presently deployed to shields.\n", shield);
    }

    return;
  }

  klingons_move();

  repair_damage(warp);

  z1 = FROM_FIXED00(ship_y);
  z2 = FROM_FIXED00(ship_x);
  quad[z1-1][z2-1] = Q_SPACE;


  c2 = FROM_FIXED00(c1);	/* Integer part */
  c3 = c2 + 1;		/* Next integer part */
  c4 = (c1 - TO_FIXED00(c2));	/* Fractional element in fixed point */

  x1 = 100 * c[1][c2] + (c[1][c3] - c[1][c2]) * c4;
  x2 = 100 * c[2][c2] + (c[2][c3] - c[2][c2]) * c4;

  x = ship_y;
  y = ship_x;

  for (i = 1; i <= n; i++) {
//		trek_printf(">%d %d %d %d %d\n",
//			i, ship_y, ship_x, x1, x2);
    ship_y = ship_y + x1;
    ship_x = ship_x + x2;

//		trek_printf("=%d %d %d %d %d\n",
//			i, ship_y, ship_x, x1, x2);

    z1 = FROM_FIXED00(ship_y);
    z2 = FROM_FIXED00(ship_x);	/* ?? cint100 ?? */

    /* Changed quadrant */
    if (z1 < 1 || z1 >= 9 || z2 < 1 || z2 >= 9) {
      uint8_t outside = 0;		/* Outside galaxy flag */
      uint8_t quad_y_old = quad_y;
      uint8_t quad_x_old = quad_x;

      x = (800 * quad_y) + x + (n * x1);
      y = (800 * quad_x) + y + (n * x2);

      //	trek_printf("X %d Y %d\n", x, y);

      quad_y = x / 800;	/* Fixed point to int and divide by 8 */
      quad_x = y / 800;	/* Ditto */

      //	trek_printf("Q %d %d\n", quad_y, quad_x);

      ship_y = x - (quad_y * 800);
      ship_x = y - (quad_x * 800);

      //	trek_printf("S %d %d\n", ship_y, ship_x);

      if (ship_y < 100) {
        quad_y = quad_y - 1;
        ship_y = ship_y + 800;
      }

      if (ship_x < 100) {
        quad_x = quad_x - 1;
        ship_x = ship_x + 800;
      }

      /* check if outside galaxy */

      if (quad_y < 1) {
        outside = 1;
        quad_y = 1;
        ship_y = 100;
      }

      if (quad_y > 8) {
        outside = 1;
        quad_y = 8;
        ship_y = 800;
      }

      if (quad_x < 1) {
        outside = 1;
        quad_x = 1;
        ship_x = 100;
      }

      if (quad_x > 8) {
        outside = 1;
        quad_x = 8;
        ship_x = 800;
      }

      if (outside == 1) {
        /* Mostly showfile ? FIXME */
        trek_printf("LT. Uhura reports:\n"
           "  Message from Starfleet Command:\n\n"
           "  Permission to attempt crossing of galactic perimeter\n"
           "  is hereby *denied*. Shut down your engines.\n\n"
           "Chief Engineer Scott reports:\n"
           "  Warp Engines shut down at sector %d, "
           "%d of quadrant %d, %d.\n\n",
               FROM_FIXED00(ship_y),
               FROM_FIXED00(ship_x), quad_y, quad_x);
      }
      maneuver_energy(n);

      /* this section has a different order in the original.
         t = t + 1;

         if (t > time_start + time_up)
         end_of_time();
       */

      if (FROM_FIXED(stardate) > time_start + time_up)
        end_of_time();

      if (quad_y != quad_y_old || quad_x != quad_x_old) {
        stardate = stardate + TO_FIXED(1);
        new_quadrant();
      }
      complete_maneuver(warp, n);
      return;
    }

    if (quad[z1-1][z2-1] != Q_SPACE) {	/* Sector not empty */
      ship_y = ship_y - x1;
      ship_x = ship_x - x2;
      trek_printf("Warp Engines shut down at sector "
             "%d, %d due to bad navigation.\n\n", z1, z2);
      i = n + 1;
    }
  }
  complete_maneuver(warp, n);
}

static void complete_maneuver(uint16_t warp, uint16_t n)
{
  uint16_t time_used;

  place_ship();
  maneuver_energy(n);

  time_used = TO_FIXED(1);

  /* Ick FIXME - re really want to tidy up time to FIXED00 */
  if (warp < 100)
    time_used = TO_FIXED(FROM_FIXED00(warp));

  stardate += time_used;

  if (FROM_FIXED(stardate) > time_start + time_up)
    end_of_time();

  short_range_scan();
}


static void maneuver_energy(uint16_t n)
{
  energy -= n + 10;

  if (energy >= 0)
    return;

  /* FIXME:
     This never occurs with the nav code as is - ancient trek versions
     included shield power in movement allowance if shield control
     was undamaged */
  trek_puts("Shield Control supplies energy to complete maneuver.\n");

  shield = shield + energy;
  energy = 0;

  if (shield <= 0)
    shield = 0;
}

static const char *srs_1 = "------------------------";

static const char *tilestr[] = {
  " . ",
  " * ",
  ">!<",
  "+K+",
  "<*>"
};

static void short_range_scan(void)
{
  register int i, j;
  char *sC = "GREEN";

  if (energy < energy0 / 10)
    sC = "YELLOW";

  if (klingons > 0)
    sC = "*RED*";

  docked = 0;

  for (i = (int) (FROM_FIXED00(ship_y) - 1); i <= (int) (FROM_FIXED00(ship_y) + 1); i++)
    for (j = (int) (FROM_FIXED00(ship_x) - 1); j <= (int) (FROM_FIXED00(ship_x) + 1); j++)
      if (i >= 1 && i <= 8 && j >= 1 && j <= 8) {
        if (quad[i-1][j-1] == Q_BASE) {
          docked = 1;
          sC = "DOCKED";
          energy = energy0;
          torps = torps0;
          trek_puts("Shields dropped for docking purposes.");
          shield = 0;
        }
      }

  if (damage[2] < 0) {
    trek_puts("\n*** Short Range Sensors are out ***");
    return;
  }

  trek_puts(srs_1);
  for (i = 0; i < 8; i++) {
    for (j = 0; j < 8; j++)
      trek_printf("%s", (tilestr[quad[i][j]]));

    if (i == 0)
      trek_printf("    Stardate            %d\n", FROM_FIXED(stardate));
    if (i == 1)
      trek_printf("    Condition           %s\n", sC);
    if (i == 2)
      trek_printf("    Quadrant            %d, %d\n", quad_y, quad_x);
    if (i == 3)
      trek_printf("    Sector              %d, %d\n", FROM_FIXED00(ship_y), FROM_FIXED00(ship_x));
    if (i == 4)
      trek_printf("    Photon Torpedoes    %d\n", torps);
    if (i == 5)
      trek_printf("    Total Energy        %d\n", energy + shield);
    if (i == 6)
      trek_printf("    Shields             %d\n", shield);
    if (i == 7)
      trek_printf("    Klingons Remaining  %d\n", klingons_left);
  }
  trek_puts(srs_1);
  trek_putchar('\n');

  return;
}

static const char *lrs_1 = "-------------------\n";

static void put1bcd(uint8_t v)
{
  v &= 0x0F;
  trek_putchar('0' + v);
}

static void putbcd(uint16_t x)
{
  put1bcd(x >> 8);
  put1bcd(x >> 4);
  put1bcd(x);
}

static void long_range_scan(void)
{
  register int i, j;

  if (inoperable(3))
    return;

  trek_printf("Long Range Scan for Quadrant %d, %d\n\n", quad_y, quad_x);

  for (i = quad_y - 1; i <= quad_y + 1; i++) {
    trek_printf("%s:", lrs_1);
    for (j = quad_x - 1; j <= quad_x + 1; j++) {
      trek_putchar(' ');
      if (i > 0 && i <= 8 && j > 0 && j <= 8) {
        map[i][j] |= MAP_VISITED;
        putbcd(map[i][j]);
      } else
        trek_printf("***");
      trek_printf(" :");
    }
    trek_putchar('\n');
  }

  trek_printf("%s\n", lrs_1);
}

static uint8_t no_klingon(void)
{
  if (klingons <= 0) {
    trek_puts("Science Officer Spock reports:\n"
         "  'Sensors show no enemy ships in this quadrant'\n");
    return 1;
  }
  return 0;
}

static void wipe_klingon(struct klingon *k)
{
  quad[k->y-1][k->x-1] = Q_SPACE;
}

static void phaser_control(void)
{
  register int i;
  int32_t phaser_energy;
  uint32_t h1;
  int h;
  struct klingon *k = kdata;

  if (inoperable(4))
    return;

  if (no_klingon())
    return;

  /* There's Klingons on the starboard bow... */
  if (damage[8] < 0)
    trek_puts("Computer failure hampers accuracy.");

  trek_printf("Phasers locked on target;\n"
         "Energy available = %d units\n\n"
         "Number of units to fire: ", energy);

  phaser_energy = input_int();

  if (phaser_energy <= 0)
    return;

  if (energy - phaser_energy < 0) {
    trek_puts("Not enough energy available.\n");
    return;
  }

  energy -=  phaser_energy;

  /* We can fire up to nearly 3000 points of energy so we do this
     bit in 32bit math */

  if (damage[8] < 0)
    phaser_energy *= get_rand(100);
  else
    phaser_energy *= 100;

  h1 = phaser_energy / klingons;

  for (i = 0; i <= 2; i++) {
    if (k->energy > 0) {
      /* We are now 32bit with four digits after the point */
      h1 = h1 * (get_rand(100) + 200);

      /* Takes us down to 2 digit accuracy */
      h1 /= distance_to(k);

      if (h1 <= 15 * k->energy) {	/* was 0.15 */
        trek_printf("Sensors show no damage to enemy at "
               "%d, %d\n\n", k->y, k->x);
      } else {
        h = FROM_FIXED00(h1);
        k->energy -= h;
        trek_printf("%d unit hit on Klingon at sector "
               "%d, %d\n",
          h, k->y, k->x);
        if (k->energy <= 0) {
          trek_puts("*** Klingon Destroyed ***\n");
          klingons--;
          klingons_left--;
          wipe_klingon(k);
          k->energy = 0;
          /* Minus a Klingon.. */
          map[quad_y][quad_x] -= 0x100;
          if (klingons_left <= 0)
            won_game();
        } else
          trek_printf("   (Sensors show %d units remaining.)\n\n", k->energy);
      }
    }
    k++;
  }

  klingons_shoot();
}

static void photon_torpedoes(void)
{
  int x3, y3;
  int16_t c1;
  int c2, c3, c4;
  int16_t x, y, x1, x2;

  if (torps <= 0) {
    trek_puts("All photon torpedoes expended");
    return;
  }

  if (inoperable(5))
    return;

  trek_puts(compass_txt);
  trek_printf("Course (0-9): ");

  c1 = input_f00();

  if (c1 == 900)
    c1 = 100;

  if (c1 < 100 || c1 >= 900) {
    trek_printf("Ensign Chekov%s", inc_1);
    return;
  }

  /* FIXME: energy underflow check ? */
  energy = energy - 2;
  torps--;

  c2 = FROM_FIXED00(c1);	/* Integer part */
  c3 = c2 + 1;		/* Next integer part */
  c4 = (c1 - TO_FIXED00(c2));	/* Fractional element in fixed point */

  x1 = 100 * c[1][c2] + (c[1][c3] - c[1][c2]) * c4;
  x2 = 100 * c[2][c2] + (c[2][c3] - c[2][c2]) * c4;

  /* The basic code is very confused about what is X and what is Y */
  x = ship_y + x1;
  y = ship_x + x2;

  x3 = FROM_FIXED00(x);
  y3 = FROM_FIXED00(y);

  trek_puts("Torpedo Track:");

  while (x3 >= 1 && x3 <= 8 && y3 >= 1 && y3 <= 8) {
    uint8_t p;

    trek_printf("    %d, %d\n", x3, y3);

    p = quad[x3-1][y3-1];
    /* In certain corner cases the first trace we'll step is
       ourself. If so treat it as space */
    if (p != Q_SPACE && p != Q_SHIP) {
      torpedo_hit(x3, y3);
      klingons_shoot();
      return;
    }

    x = x + x1;
    y = y + x2;

    x3 = FROM_FIXED00(x);
    y3 = FROM_FIXED00(y);
  }

  trek_puts("Torpedo Missed\n");

  klingons_shoot();
}

static void torpedo_hit(uint8_t yp, uint8_t xp)
{
  int i;
  struct klingon *k;

  switch(quad[yp-1][xp-1]) {
  case Q_STAR:
    trek_printf("Star at %d, %d absorbed torpedo energy.\n\n", yp, xp);
    return;
  case Q_KLINGON:
    trek_puts("*** Klingon Destroyed ***\n");
    klingons--;
    klingons_left--;

    if (klingons_left <= 0)
      won_game();

    k = kdata;
    for (i = 0; i <= 2; i++) {
      if (yp == k->y && xp == k->x)
        k->energy = 0;
      k++;
    }
    map[quad_y][quad_x] -= 0x100;
    break;
  case Q_BASE:					
    trek_puts("*** Starbase Destroyed ***");
    starbases--;
    starbases_left--;

    if (starbases_left <= 0 && klingons_left <= FROM_FIXED(stardate) - time_start - time_up) {
      /* showfile ? FIXME */
      trek_puts("That does it, Captain!!"
           "You are hereby relieved of command\n"
           "and sentenced to 99 stardates of hard"
           "labor on Cygnus 12!!\n");
      resign_commision();
    }

    trek_puts("Starfleet Command reviewing your record to consider\n"
         "court martial!\n");

    docked = 0;		/* Undock */
    map[quad_y][quad_x] -= 0x10;
    break;
  }
  quad[yp-1][xp-1] = Q_SPACE;
}

static void damage_control(void)
{
  int16_t repair_cost = 0;
  register int i, j;
  const char * dstr;
  int dslen;

  if (damage[6] < 0)
    trek_puts("Damage Control report not available.");

  /* Offer repair if docked */
  if (docked) {
    /* repair_cost is x.xx fixed point */
    repair_cost = 0;
    for (i = 1; i <= 8; i++)
      if (damage[i] < 0)
        repair_cost = repair_cost + 10;

    if (repair_cost) {
      repair_cost = repair_cost + d4;
      if (repair_cost >= 100)
        repair_cost = 90;	/* 0.9 */

      trek_printf("\nTechnicians standing by to effect repairs to your"
             "ship;\nEstimated time to repair: ");
      print100(repair_cost);
      trek_printf(" stardates.\n");
      trek_printf("Will you authorize the repair order (y/N)? ");

      if (yesno()) {
        for (i = 1; i <= 8; i++)
          if (damage[i] < 0)
            damage[i] = 0;

        /* Work from two digit to one digit. We might actually
           have to give in and make t a two digt offset from
           a saved constant base only used in printing to
           avoid that round below FIXME */
        stardate += (repair_cost + 5)/10 + 1;
      }
      return;
    }
  }

  if (damage[6] < 0)
    return;

  trek_printf("Device            State of Repair\n");

  for (i = 1; i <= 8; i++) {
    dstr = get_device_name(i);
    dslen = strlen(dstr);
    trek_printf("%s", dstr);
    for (j = 1; j < 25 - dslen; j++)
      trek_putchar(' ');
    print100(damage[i]);
    trek_printf("\n");
  }
}

static void shield_control(void)
{
  int i;

  if (inoperable(7))
    return;

  trek_printf("Energy available = %d\n\n"
         "Input number of units to shields: ", energy + shield);

  i = input_int();

  if (i < 0 || shield == i) {
unchanged:
    trek_puts("<Shields Unchanged>\n");
    return;
  }

  if (i >= energy + shield) {
    trek_puts("Shield Control Reports:\n"
         "  'This is not the Federation Treasury.'");
    goto unchanged;
  }

  energy = energy + shield - i;
  shield = i;

  trek_printf("Deflector Control Room report:\n"
         "  'Shields now at %d units per your command.'\n\n", shield);
}

static void library_computer(void)
{

  if (inoperable(8))
    return;

  trek_printf("Computer active and awating command: ");

  switch(input_int()) {
    /* -1 means 'typed nothing or junk */
    case -1:
      break;
    case 0:
      galactic_record();
      break;
    case 1:
      status_report();
      break;
    case 2:
      torpedo_data();
      break;
    case 3:
      nav_data();
      break;
    case 4:
      dirdist_calc();
      break;
    case 5:
      galaxy_map();
      break;
    default:
      /* FIXME: showfile */
      trek_puts("Functions available from Library-Computer:\n\n"
           "   0 = Cumulative Galactic Record\n"
           "   1 = Status Report\n"
           "   2 = Photon Torpedo Data\n"
           "   3 = Starbase Nav Data\n"
           "   4 = Direction/Distance Calculator\n"
           "   5 = Galaxy 'Region Name' Map\n");
  }
}

static const char *gr_1 = "   ----- ----- ----- ----- ----- ----- ----- -----\n";

static void galactic_record(void)
{
  int i, j;

  trek_printf("\n     Computer Record of Galaxy for Quadrant %d,%d\n\n", quad_y, quad_x);
  trek_puts("     1     2     3     4     5     6     7     8");

  for (i = 1; i <= 8; i++) {
    trek_printf("%s%d", gr_1, i);

    for (j = 1; j <= 8; j++) {
      trek_printf("   ");

      if (map[i][j] & MAP_VISITED)
        putbcd(map[i][j]);
      else
        trek_printf("***");
    }
    trek_putchar('\n');
  }

  trek_printf("%s\n", gr_1);
}

static const char *str_s = "s";

static void status_report(void)
{
  const char *plural = str_s + 1;
  uint16_t left = TO_FIXED(time_start + time_up) - stardate;

  trek_puts("   Status Report:\n");

  if (klingons_left > 1)
    plural = str_s;

  /* Assumes fixed point is single digit fixed */
  trek_printf("Klingon%s Left: %d\n"
         "Mission must be completed in %d.%d stardates\n",
    plural, klingons_left,
    FROM_FIXED(left), left%10);

  if (starbases_left < 1) {
    trek_puts("Your stupidity has left you on your own in the galaxy\n"
         " -- you have no starbases left!\n");
  } else {
    plural = str_s;
    if (starbases_left < 2)
      plural++;

    trek_printf("The Federation is maintaining %d starbase%s in the galaxy\n\n", starbases_left, plural);
  }
}

static void torpedo_data(void)
{
  int i;
  const char *plural = str_s + 1;
  struct klingon *k;

  /* Need to also check sensors here ?? */
  if (no_klingon())
    return;

  if (klingons > 1)
    plural--;

  trek_printf("From Enterprise to Klingon battlecriuser%s:\n\n", plural);

  k = kdata;
  for (i = 0; i <= 2; i++) {
    if (k->energy > 0) {
      compute_vector(TO_FIXED00(k->y),
               TO_FIXED00(k->x),
               ship_y, ship_x);
    }
    k++;
  }
}

static void nav_data(void)
{
  if (starbases <= 0) {
    trek_puts("Mr. Spock reports,\n"
         "  'Sensors show no starbases in this quadrant.'\n");
    return;
  }
  compute_vector(TO_FIXED00(base_y), TO_FIXED00(base_x), ship_y, ship_x);
}

/* Q: do we want to support fractional co-ords ? */
static void dirdist_calc(void)
{
  int16_t c1, a, w1, x;
  trek_printf("Direction/Distance Calculator\n"
         "You are at quadrant %d,%d sector %d,%d\n\n"
         "Please enter initial X coordinate: ",
         quad_y, quad_x,
         FROM_FIXED00(ship_y), FROM_FIXED00(ship_x));

  c1 = TO_FIXED00(input_int());
  if (c1 < 0 || c1 > 900 )
    return;

  trek_printf("Please enter initial Y coordinate: ");
  a = TO_FIXED00(input_int());
  if (a < 0 || a > 900)
    return;

  trek_printf("Please enter final X coordinate: ");
  w1 = TO_FIXED00(input_int());
  if (w1 < 0 || w1 > 900)
    return;

  trek_printf("Please enter final Y coordinate: ");
  x = TO_FIXED00(input_int());
  if (x < 0 || x > 900)
    return;
  compute_vector(w1, x, c1, a);
}

static const char *gm_1 = "  ----- ----- ----- ----- ----- ----- ----- -----\n";

static void galaxy_map(void)
{
  int i, j, j0;

  trek_printf("\n                   The Galaxy\n\n");
  trek_printf("    1     2     3     4     5     6     7     8\n");

  for (i = 1; i <= 8; i++) {
    trek_printf("%s%d ", gm_1, i);

    quadrant_name(1, i, 1);

    j0 = (int) (11 - (strlen(quadname) / 2));

    for (j = 0; j < j0; j++)
      trek_putchar(' ');

    trek_printf("%s", quadname);

    for (j = 0; j < j0; j++)
      trek_putchar(' ');

    if (!(strlen(quadname) % 2))
      trek_putchar(' ');

    quadrant_name(1, i, 5);

    j0 = (int) (12 - (strlen(quadname) / 2));

    for (j = 0; j < j0; j++)
      trek_putchar(' ');

    trek_puts(quadname);
  }

  trek_puts(gm_1);

}

static void compute_vector(int16_t w1, int16_t x, int16_t c1, int16_t a)
{
  uint32_t xl, al;

  trek_printf("  DIRECTION = ");
  /* All this is happening in fixed point */
  x = x - a;
  a = c1 - w1;

  xl = abs(x);
  al = abs(a);

  if (x < 0) {
    if (a > 0) {
      c1 = 300;
estimate2:
    /* Multiply the top half by 100 to keep in fixed0 */
      if (al >= xl)
        print100(c1 + ((xl * 100) / al));
      else
        print100(c1 + ((((xl * 2) - al) * 100)  / xl));

      trek_printf("  DISTANCE = ");
      print100((x > a) ? x : a);
      trek_printf("\n\n");
      return;
    } else if (x != 0){
      c1 = 500;
      goto estimate1;
      return;
    } else {
      c1 = 700;
      goto estimate2;
    }
  } else if (a < 0) {
    c1 = 700;
    goto estimate2;
  } else if (x > 0) {
    c1 = 100;
    goto estimate1;
  } else if (a == 0) {
    c1 = 500;
    goto estimate1;
  } else {
    c1 = 100;
estimate1:
    /* Multiply the top half by 100 as well so that we keep it in fixed00
       format. Our larget value is int 9 (900) so we must do this 32bit */
    if (al <= xl)
      print100(c1 + ((al * 100) / xl));
    else
      print100(c1 + ((((al * 2) - xl) * 100) / al));
    trek_printf("  DISTANCE = ");
    print100((xl > al) ? xl : al);
    trek_printf("\n\n");
  }
}
static void ship_destroyed(void)
{
  trek_puts("The Enterprise has been destroyed. "
       "The Federation will be conquered.\n");

  end_of_time();
}

static void end_of_time(void)
{
  trek_printf("It is stardate %d.\n\n",  FROM_FIXED(stardate));

  resign_commision();
}

static void resign_commision(void)
{
  trek_printf("There were %d Klingon Battlecruisers left at the"
         " end of your mission.\n\n", klingons_left);

  end_of_game();
}

static void won_game(void)
{
  trek_puts("Congratulations, Captain!  The last Klingon Battle Cruiser\n"
       "menacing the Federation has been destoyed.\n");

  if (FROM_FIXED(stardate) - time_start > 0) {
    trek_printf("Your efficiency rating is ");
    print100(square00(TO_FIXED00(total_klingons)/(FROM_FIXED(stardate) - time_start)));
    trek_printf("\n");
  }

  end_of_game();
}

static void end_of_game(void)
{
  char x[8];
  if (starbases_left > 0) {
    /* FIXME: showfile ? */
    trek_printf("The Federation is in need of a new starship commander"
         " for a similar mission.\n"
         "If there is a volunteer, step forward and enter 'aye': ");

    trek_getsn(x, sizeof(x));
    if (!strncmp(x, "aye", 3))
      new_game();
  }

  exited = 1;
}

static void klingons_move(void)
{
  int i;
  struct klingon *k = kdata;

  for (i = 0; i <= 2; i++) {
    if (k->energy > 0) {
      wipe_klingon(k);

      find_set_empty_place(Q_KLINGON, &k->y, &k->x);
    }
    k++;
  }

  klingons_shoot();
}

static const char *dcr_1 = "Damage Control report:";

static void klingons_shoot(void)
{
  uint32_t h;
  uint8_t i;
  struct klingon *k = kdata;

  if (klingons <= 0)
    return;

  if (docked) {
    trek_puts("Starbase shields protect the Enterprise\n");
    return;
  }

  for (i = 0; i <= 2; i++) {
    if (k->energy > 0) {
      h = k->energy * (200UL + get_rand(100));
      h *= 100;	/* Ready for division in fixed */
      h /= distance_to(k);
      /* Takes us back into FIXED00 */
      shield = shield - FROM_FIXED00(h);

      k->energy = (k->energy * 100) / (300 + get_rand(100));

      trek_printf("%d unit hit on Enterprise from sector "
             "%d, %d\n", (unsigned)FROM_FIXED00(h), k->y, k->x);

      if (shield <= 0) {
        trek_putchar('\n');
        ship_destroyed();
      }

      trek_printf("    <Shields down to %d units>\n\n", shield);

      if (h >= 20) {
        /* The check in basic is float and is h/s >.02. We
           have to use 32bit values here to avoid an overflow
           FIXME: use a better algorithm perhaps ? */
        uint32_t ratio = ((uint32_t)h)/shield;
        if (get_rand(10) <= 6 && ratio > 2) {
          uint8_t r = rand8();
          /* The original basic code computed h/s in
             float form the C conversion broke this. We correct it in the fixed
             point change */
          damage[r] -= ratio + get_rand(50);

          /* FIXME: can we use dcr_1 here ?? */
          trek_printf("Damage Control reports\n"
                 "   '%s' damaged by hit\n\n", get_device_name(r));
        }
      }
    }
    k++;
  }
}

static void repair_damage(uint16_t warp)
{
  int i;
  int d1;
  uint16_t repair_factor;		/* Repair Factor */

  repair_factor = warp;
  if (warp >= 100)
    repair_factor = TO_FIXED00(1);

  for (i = 1; i <= 8; i++) {
    if (damage[i] < 0) {
      damage[i] = damage[i] + repair_factor;
      if (damage[i] > -10 && damage[i] < 0)	/* -0.1 */
        damage[i] = -10;
      else if (damage[i] >= 0) {
        if (d1 != 1) {
          d1 = 1;
          trek_puts(dcr_1);
        }
        trek_printf("    %s repair completed\n\n",
          get_device_name(i));
        damage[i] = 0;
      }
    }
  }

  if (get_rand(10) <= 2) {
    uint8_t r = rand8();

    if (get_rand(10) < 6) {
      /* Working in 1/100ths */
      damage[r] -= (get_rand(500) + 100);
      trek_puts(dcr_1);
      trek_printf("    %s damaged\n\n", get_device_name(r));
    } else {
      /* Working in 1/100ths */
      damage[r] += get_rand(300) + 100;
      trek_puts(dcr_1);
      trek_printf("    %s state of repair improved\n\n",
          get_device_name(r));
    }
  }
}

/* Misc Functions and Subroutines
   Returns co-ordinates r1/r2 and for now z1/z2 */

static void find_set_empty_place(uint8_t t, uint8_t *z1, uint8_t *z2)
{
  uint8_t r1, r2;
  do {
    r1 = rand8();
    r2 = rand8();
  } while (quad[r1-1][r2-1] != Q_SPACE );
  quad[r1-1][r2-1] = t;
  if (z1)
    *z1 = r1;
  if (z2)
    *z2 = r2;
}

static const char *device_name[] = {
  "", "Warp engines", "Short range sensors", "Long range sensors",
  "Phaser control", "Photon tubes", "Damage control", "Shield control",
  "Library computer"
};

static const char *get_device_name(int n)
{
  if (n < 0 || n > 8)
    n = 0;
  return device_name[n];
}

static const char *quad_name[] = { "",
  "Antares", "Rigel", "Procyon", "Vega", "Canopus", "Altair",
  "Sagittarius", "Pollux", "Sirius", "Deneb", "Capella",
  "Betelgeuse", "Aldebaran", "Regulus", "Arcturus", "Spica"
};

static void quadrant_name(uint8_t small, uint8_t y, uint8_t x)
{

  static char *sect_name[] = { "", " I", " II", " III", " IV" };

  if (y < 1 || y > 8 || x < 1 || x > 8)
    strcpy(quadname, "Unknown");

  if (x <= 4)
    strcpy(quadname, quad_name[y]);
  else
    strcpy(quadname, quad_name[y + 8]);

  if (small != 1) {
    if (x > 4)
      x = x - 4;
    strcat(quadname, sect_name[x]);
  }

  return;
}

/* An unsigned sqrt is all we need.

   What we are actually doing here is a smart version of calculating n^2
   repeatedly until we find the right one */
static int16_t isqrt(int16_t i) {
  uint16_t b = 0x4000, q = 0, r = i, t;
  while (b) {
    t = q + b;
    q >>= 1;
    if (r >= t) {
      r -= t;
      q += b;
    }
    b >>= 2;
  }
  return q;
}

static int square00(int16_t t) {
  if (abs(t) > 181) {
    t /= 10;
    t *= t;
  } else {
    t *= t;
    t /= 100;
  }
  return t;
}

/* Return the distance to an object in x.xx fixed point */
static int distance_to(struct klingon *k)
{
  uint16_t j;

  /* We do the squares in fixed point maths */
  j = square00(TO_FIXED00(k->y) - ship_y);
  j += square00(TO_FIXED00(k->x) - ship_x);

  /* Find the integer square root */
  j = isqrt(j);
  /* Correct back into 0.00 fixed point */
  j *= 10;

  return j;
}

/* Round off floating point numbers instead of truncating */

static int cint100(int16_t d) {
  return (d + 50) / 100;
}

/* Functions below added by Kirill Levchenko. */

static uint64_t rndst = 0;
#define RAND_MAX 0x7fffffff

void rand_init(void) {
  // Initialize PRNG from goldfish_rtc device; must read lo then hi.
    rndst = (uint64_t)*(volatile uint32_t*)0x101000 << 32;
    rndst |= *(volatile uint32_t*)0x101004;
}

int rand(void) {
  rndst *= 25214903917UL;
  rndst += 11;
  return rndst >> 12;
}

static int abs(int x) {
  return (x < 0) ? -x : x;
}

int atoi(const char *s) {
  int val = 0;
  int neg = 0;

  while (*s == ' ')
    s += 1;
  
  if (*s == '-')
    neg = -1;
  
  while ('0' <= *s && *s <= '9') {
    val = 10 * val + *s - '0';
  s += 1;
  }
  
  return neg ? -val : val;
}


char * strcpy(char * s0, const char * s1) {
  char * sout = s0;
  while (*s1 != '\0')
    *s0++ = *s1++;
  *s0 = '\0';
  return sout;
}

char * strcat(char * s0, const char * s1) {
  char * sout = s0;
  while (*s0 != '\0')
    s0++;
  while (*s1 != '\0')
    *s0++ = *s1++;
  *s0 = '\0';
  return sout;
}

void trek_putchar(char c) {
  static char cprev = '\0';

  switch (c) {
  case '\r':
    com_putc(trek_comno, c);
    com_putc(trek_comno, '\n');
    break;
  case '\n':
    if (cprev != '\r')
      com_putc(trek_comno, '\r');
    // nobreak
  default:
    com_putc(trek_comno, c);
    break;
  }

  cprev = c;
}

char trek_getchar(void) {
  static char cprev = '\0';
  char c;

  // Convert \r followed by any number of \n to just \n

  do {
    c = com_getc(trek_comno);
  } while (c == '\n' && cprev == '\r');
  
  cprev = c;

  if (c == '\r')
    return '\n';
  else
    return c;
}

void trek_puts(const char * str) {
  while (*str != '\0')
    trek_putchar(*str++);
  trek_putchar('\n');
}

char * trek_getsn(char * buf, size_t n) {
  char * p = buf;
  char c;

  for (;;) {
    c = trek_getchar();

    switch (c) {
    case '\r':
      break;		
    case '\n':
#ifdef TREK_RAW_MODE
      trek_putchar('\n');
#endif
      *p = '\0';
      return buf;
    case '\b':
    case '\177':
      if (p != buf) {
#ifdef TREK_RAW_MODE
        trek_putchar('\b');
        trek_putchar(' ');
        trek_putchar('\b');
#endif
        p -= 1;
        n += 1;
      }
      break;
    default:
      if (n > 1) {
#ifdef TREK_RAW_MODE
        trek_putchar(c);
#endif
        *p++ = c;
        n -= 1;
      } else {
#ifdef TREK_RAW_MODE
        trek_putchar('\a'); // bell
#endif
      }

      break;
    }
  }

  return buf;
}

void trek_printf(const char * fmt, ...) {
  va_list ap;

  va_start(ap, fmt);
  vgprintf((void*)trek_putchar, NULL, fmt, ap);
  va_end(ap);
}

void trek_wait_enter(void) {
  char buf[1];
  trek_getsn(buf, sizeof(buf));
}

static const char instr_txt[] = 
"1. When you see _Command?_ printed, enter one of the legal commands\n"
"   (nav, srs, lrs, pha, tor, she, dam, com, or xxx).\n"
"\n"
"2. If you should type in an illegal command, you'll get a short list of\n"
"   the legal commands printed out.\n"
"\n"
"3. Some commands require you to enter data (for example, the 'nav' command\n"
"   comes back with 'Course(1-9) ?'.) If you type in illegal data (like\n"
"   negative numbers), that command will be aborted.\n"
"\n"
"  The galaxy is divided into an 8 X 8 quadrant grid, and each quadrant\n"
"is further divided into an 8 x 8 sector grid.\n"
"\n"
"  You will be assigned a starting point somewhere in the galaxy to begin\n"
"a tour of duty as commander of the starship _Enterprise_; your mission:\n"
"to seek out and destroy the fleet of Klingon warships which are menacing\n"
"the United Federation of Planets.\n"
"\n"
"  You have the following commands available to you as Captain of the Starship\n"
"Enterprise:\n"
"\n"
"/nav/ Command = Warp Engine Control --\n"
"\n"
"  Course is in a circular numerical vector            4  3  2\n"
"  arrangement as shown. Integer and real               \\ | /\n"
"  values may be used. (Thus course 1.5 is               \\|/\n"
"  half-way between 1 and 2.                         5 ---+--- 1\n"
"                                                        /|\\\n"
"  Values may approach 9.0, which itself is             / | \\\n"
"  equivalent to 1.0.                                  6  7  8\n"
"\n"
"  One warp factor is the size of one quadrant.        COURSE\n"
"  Therefore, to get from quadrant 6,5 to 5,5\n"
"  you would use course 3, warp factor 1.\n"
"\n"
"/srs/ Command = Short Range Sensor Scan\n"
"\n"
"  Shows you a scan of your present quadrant.\n"
"\n"
"  Symbology on your sensor screen is as follows:\n"
"    <*> = Your starship's position\n"
"    +K+ = Klingon battlecruiser\n"
"    >!< = Federation starbase (Refuel/Repair/Re-Arm here)\n"
"     *  = Star\n"
"\n"
"  A condensed 'Status Report' will also be presented.\n"
"\n"
"/lrs/ Command = Long Range Sensor Scan\n"
"\n"
"  Shows conditions in space for one quadrant on each side of the Enterprise\n"
"  (which is in the middle of the scan). The scan is coded in the form /###/\n"
"  where the units digit is the number of stars, the tens digit is the number\n"
"  of starbases, and the hundreds digit is the number of Klingons.\n"
"\n"
"  Example - 207 = 2 Klingons, No Starbases, & 7 stars.\n"
"\n"
"/pha/ Command = Phaser Control.\n"
"\n"
"  Allows you to destroy the Klingon Battle Cruisers by zapping them with\n"
"  suitably large units of energy to deplete their shield power. (Remember,\n"
"  Klingons have phasers, too!)\n"
"\n"
"/tor/ Command = Photon Torpedo Control\n"
"\n"
"  Torpedo course is the same  as used in warp engine control. If you hit\n"
"  the Klingon vessel, he is destroyed and cannot fire back at you. If you\n"
"  miss, you are subject to the phaser fire of all other Klingons in the\n"
"  quadrant.\n"
"\n"
"  The Library-Computer (/com/ command) has an option to compute torpedo\n"
"  trajectory for you (option 2).\n"
"\n"
"/she/ Command = Shield Control\n"
"\n"
"  Defines the number of energy units to be assigned to the shields. Energy\n"
"  is taken from total ship's energy. Note that the status display total\n"
"  energy includes shield energy.\n"
"\n"
"/dam/ Command = Damage Control report\n"
"  Gives the state of repair of all devices. Where a negative 'State of Repair'\n"
"  shows that the device is temporarily damaged.\n"
"\n"
"/com/ Command = Library-Computer\n"
"  The Library-Computer contains six options:\n"
"  Option 0 = Cumulative Galactic Record\n"
"    This option shows computer memory of the results of all previous\n"
"    short and long range sensor scans.\n"
"  Option 1 = Status Report\n"
"    This option shows the number of Klingons, stardates, and starbases\n"
"    remaining in the game.\n"
"  Option 2 = Photon Torpedo Data\n"
"    Which gives directions and distance from Enterprise to all Klingons\n"
"    in your quadrant.\n"
"  Option 3 = Starbase Nav Data\n"
"    This option gives direction and distance to any starbase in your\n"
"    quadrant.\n"
"  Option 4 = Direction/Distance Calculator\n"
"    This option allows you to enter coordinates for direction/distance\n"
"    calculations.\n"
"  Option 5 = Galactic /Region Name/ Map\n"
"    This option prints the names of the sixteen major galactic regions\n"
"    referred to in the game.\n";

static const char splash_txt[] = "\n"
" *************************************\n"
" *                                   *\n"
" *                                   *\n"
" *      * * Super Star Trek * *      *\n"
" *                                   *\n"
" *                                   *\n"
" *************************************\n"
"\n\n\n\n";

static const char logo_txt[] = "\n\n\n\n\n"
"                         ------*------\n"
"         -------------   `---  ------'\n"
"         `-------- --'      / /\n"
"                  \\\\-------  --\n"
"                  '-----------'\n"
"\n"
"       The USS Enterprise --- NCC - 1701";


static const char compass_txt[] =
"\n"
"  4  3  2\n"
"   \\ | /\n"
"    \\|/\n"
"5 ---+--- 1\n"
"    /|\\\n"
"   / | \\\n"
"  6  7  8\n";