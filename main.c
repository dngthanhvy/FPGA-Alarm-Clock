#include <system.h>
#include <stdio.h>
#include <unistd.h>
#include <altera_avalon_pio_regs.h>
#include <math.h>

// ****************************** Macros ************************************************************
#define C3	130
#define D3	146
#define E3	164
#define F3	174
#define G3	196
#define A3	220
#define B3	246

#define C4	261
#define D4	293
#define E4	329
#define F4	349
#define G4	392
#define A4	440
#define B4	493

#define C5	523
#define D5	587
#define E5	659
#define F5	698
#define G5	783
#define A5	880
#define B5	987

// ****************************** Déclaration des variables *****************************************

// Partie timer
volatile unsigned int timer_sec = TIMER_1S_BASE; //timer base d'une seconde
volatile unsigned int timer_alarme = TIMER_10HZ_BASE;
volatile unsigned int timer_poussoir = TIMER_2HZ_BASE;
volatile unsigned int timer_tempo = SYS_CLK_TIMER_BASE;

// Partie compteur
int compteur=0;
int secondes = 0, minutes = 0, heure = 0;
int affichage_secondes, affichage_minutes, affichage_heure;

// Partie réglage
int compteur_reg=0;
int reg_secondes = 0, reg_minutes = 0, reg_heure = 0;

// Partie alarme
int alarme_minutes = 0, alarme_heure = 0;

// Partie melodies
int indicateur = 0;
int count = 0;
int melodie = 0;

int first_run = 0;
volatile int*GPIO = (int*)GPIO_BASE;

// ****************************** Déclaration des fonctions *****************************************

// Convertisseur nombre -> afficheur 7 segment
int sevenseg(int number)
{
	int sev_seg;
	if (number==0) sev_seg=0x40;
	else if (number==1) sev_seg=0x79;
	else if (number==2) sev_seg=0x24;
	else if (number==3) sev_seg=0x30;
	else if (number==4) sev_seg=0x19;
	else if (number==5) sev_seg=0x12;
	else if (number==6) sev_seg=0x02;
	else if (number==7) sev_seg=0x78;
	else if (number==8) sev_seg=0x00;
	else if (number==9) sev_seg=0x10;

	return sev_seg;
}

// "Éteint" les LED et l'afficheur 7 segments
void reset_led_hex()
{
	IOWR(HEX3_HEX0_BASE,0,~0x0);
	IOWR(LEDR_BASE,0,0x0);
	IOWR(HEX5_HEX4_BASE,0,~0x0);
}

// Initialiseur des timers
void init_timer(int timer_base,int low,int high)
{
  // Définition du registre
	IOWR(timer_base,2,low);
	IOWR(timer_base,3,high);

  // Initialisation du timer
	IOWR(timer_base,1,0x0006); // start
	IOWR(timer_base,0,0x0000); // reset
}

// Compteur secondes principal
void compteur_secondes()
{
  // Lorsque le timer a fini de compter : Registre 0 = 0x3
  if (IORD(timer_sec,0)==0x3)
  {
    // Si le compteur atteint 3600 fois 24 heures
    if (compteur == 24*60*60)
  {
    compteur = 0; // remise à zéro du compteur
  }
  else compteur++; // sinon compteur incrémenté
    IOWR(timer_sec,0,0x0);
  }
  else compteur = compteur; // lorsque le timer n'a pas fini de compter

  /* Attribution des secondes, minutes, heure en fonction du compteur
  Le compteur = le nombre total de secondes écoulées */
  secondes = compteur%60; // les secondes = reste de la division par 60 du compteur
  heure = compteur/3600; // l'heure = reste de la division entière du compteur par 3600
  minutes = (compteur/60)%60; // les minutes = reste de la division entière du compteur par 60 et encore par 60
}

// Afficheur 24h
void afficheur_24h()
{
  /* L'heure : nombre entre 0 et 24 donc la partie dizaine = heure/10 et la partie unité = heure%10
  Pareil pour les minutes et les secondes */
  affichage_heure = sevenseg(heure/10) << 8 | sevenseg(heure%10);
  affichage_minutes = sevenseg(minutes/10) << 24 | sevenseg(minutes%10) << 16;
  affichage_secondes = sevenseg(secondes/10) << 8 | sevenseg(secondes%10);

  IOWR(HEX3_HEX0_BASE,0,affichage_minutes|affichage_secondes);
  IOWR(HEX5_HEX4_BASE,0,affichage_heure);
}

// Afficheur 12h
void afficheur_12h()
{
  /* Pareil que l'afficheur 24h sauf qu'on prend le reste de la division entière de l'heure par 12*/
  affichage_heure = sevenseg((heure%12)/10) << 8 |sevenseg((heure%12)%10);
  affichage_minutes = sevenseg(minutes/10) << 24 | sevenseg(minutes%10) << 16;
  affichage_secondes = sevenseg(secondes/10) << 8 | sevenseg(secondes%10);

  IOWR(HEX3_HEX0_BASE,0,affichage_minutes|affichage_secondes);
  IOWR(HEX5_HEX4_BASE,0,affichage_heure);
}

// Régleur
void reglages()
{

  int press;
  press = IORD(BOUTONS_POUSSOIRS_BASE,3);
  IOWR(BOUTONS_POUSSOIRS_BASE,3,press);

  /* BP 1 = Minutes et BP 2 = Heure
  On ne veut pas incrémenter l'heure si les minutes atteignent 59 */

  // Appui continu d'incrémentation 0.5s
  if (IORD(timer_poussoir,0)==0x3)
  {
	  IOWR(timer_poussoir,0,0x0);
	  if (IORD(BOUTONS_POUSSOIRS_BASE,0)==0x1)
	  {
		  reg_minutes++;
	  }
	  if (IORD(BOUTONS_POUSSOIRS_BASE,0)==0x2)
	  {
		  reg_heure++;
	  }
  }

  // Appuis successifs
  else if (press == 0x1)
  {
	  reg_minutes++;
  }
  else if (press == 0x2)
  {
    reg_heure++;
  }

  // Conditions de remise à zéro
  if (reg_minutes > 59)
  {
	  reg_minutes = 0;
  }
  if (reg_heure > 23)
  {
	  reg_heure = 0;
  }
}

// Affichage régleur
void aff_reglages()
{
  int aff_reg_heure, aff_reg_minutes;

  // Vérification de l'état du SW0 pour affichage 24h ou 12h
  if ((IORD(INTERRUPTEURS_BASE,0) & 0x1) == 1)
  {
    aff_reg_heure = sevenseg((reg_heure%12)/10) << 8 |sevenseg((reg_heure%12)%10);
  }
  else
  {
    aff_reg_heure = sevenseg(reg_heure/10) << 8 | sevenseg(reg_heure%10);
  }
  aff_reg_minutes = sevenseg(reg_minutes/10) << 24 | sevenseg(reg_minutes%10) << 16 | sevenseg(0) << 8 | sevenseg(0);

  IOWR(HEX3_HEX0_BASE,0,aff_reg_minutes);
  IOWR(HEX5_HEX4_BASE,0,aff_reg_heure);
}

// Affichage alarme
void afficheur_alarme()
{
  int aff_alarme_heure, aff_alarme_minutes;

  // Vérification de l'état du SW0 pour l'affichage 24h ou 12h
  if ((IORD(INTERRUPTEURS_BASE,0) & 0x1) == 1)
  {
    aff_alarme_heure = sevenseg((alarme_heure%12)/10) << 8 |sevenseg((alarme_heure%12)%10);
  }
  else
  {
    aff_alarme_heure = sevenseg(alarme_heure/10) << 8 | sevenseg(alarme_heure%10);
  }

  aff_alarme_minutes = sevenseg(alarme_minutes/10) << 24 | sevenseg(alarme_minutes%10) << 16 | sevenseg(0) << 8 | sevenseg(0);

  IOWR(HEX3_HEX0_BASE,0,aff_alarme_minutes);
  IOWR(HEX5_HEX4_BASE,0,aff_alarme_heure);
}

// Boutons snooze 5 minutes
void snooze()
{
	int snooze;
	snooze = IORD(BOUTONS_POUSSOIRS_BASE,3);
	IOWR(BOUTONS_POUSSOIRS_BASE,3,snooze);

	if (snooze == 0x1 || snooze == 0x2)
	{
		alarme_minutes = alarme_minutes + 5;
	}

}

// ****************************** Mélodies ***************************************************

// Réglage de la mélodie voulue
void reg_melodie()
{
	  int press;

	  press = IORD(BOUTONS_POUSSOIRS_BASE,3);
	  IOWR(BOUTONS_POUSSOIRS_BASE,3,press);

	  if (press == 0x1)
	  {
		  if (melodie == 2)
		  {
			  melodie = 0;
		  }
		  else melodie++;
	  }
	  if (press == 0x2)
	  {
		  if (melodie == 0)
		  {
			  melodie = 0;
		  }
		  else melodie = melodie - 1;
	  }
}

// Affichage de la mélodie choisie
void aff_melodie()
{
	IOWR(HEX3_HEX0_BASE,0,~0x0<<24|~0x0<<16|~0x0<<8|sevenseg(melodie+1));
	IOWR(HEX5_HEX4_BASE,0,~0x0<<8|~0x0);
}

//Génération d'un signal carré
void gen_note(int freq)
{
	 /* Si la fréquence est nulle alors le GPIO est 0
	 * Sinon, on génère un signal carré de fréquence voulue
	 * L'utilisation des timer est plus facile ici avec l'opérateur =!
	 * */

	if (freq == 0)
	{
		*GPIO = 0;
	}
	else
	{
		/* Partie low et partie high du timer*/
		int low, high;
		low = (50000000/(2*freq) & 0x0000FFFF);
		high = (50000000/(2*freq)>>16 & 0xFFFF);

		/* Initialisation du timer seulement 1 fois */
		if (first_run == 0)
		{
			// init_timer(timer_alarme,0xC350,0x0000);
			init_timer(timer_alarme,low,high);
			first_run = 1;
		}
		else if (IORD(timer_alarme,0) == 0x3)
		{
			*GPIO =! *GPIO;
			IOWR(timer_alarme,0,0x0);
		}
	}
}

// Gestion de la durée de la note
void note_tempo(int tempo, int melody_size)
{
	/* A chaque impulsion du timer_tempo
	 * on incrémente count jusqu'à ce qu'on
	 * atteigne la durée voulue
	 * Dans ce cas-là count est remise à 0
	 * et on passe sur une autre colonne du tableau
	 */
	if (IORD(timer_tempo,0) == 0x3)
	{
		IOWR(timer_tempo,0,0x0);
		count++;
		if (count == tempo)
		{
			count = 0;
			indicateur++;
			if (indicateur == melody_size)
			{
				indicateur = 0;
			}
		}
	}
}

// Mélodie 1 : note simple
void melodie_1()
{
	gen_note(500);
}

// Mélodie 2 :
void melodie_2()
{
	int melodie[] =
	{
			500, 0, 700, 0, 300, 0
	};

	int tempo[] =
	{
			3, 3, 3, 3, 3, 3
	};
	int indicateur_max = (sizeof(melodie)/sizeof(melodie[0]));

	gen_note(melodie[indicateur]);
	note_tempo(tempo[indicateur], indicateur_max);
}

// Mélodie 3 :
void melodie_3()
{
	int melodie[] =
	{
			700, 0,
			200, 0
	};

	int tempo[] =
	{
			3, 1,
			3, 1
	};

	int indicateur_max = (sizeof(melodie)/sizeof(melodie[0]));

	gen_note(melodie[indicateur]);
	note_tempo(tempo[indicateur], indicateur_max);
}


// ****************************** Fonction principale *****************************************

int main()
{
  // Partie initialisation
  reset_led_hex();
  // 50000000*1s = 0x2FAF080
  // 50000000*0.5s = 0x17D7840
  // 50000000*0.1s = 0x4C4B40
  init_timer(timer_sec,0xF080,0x02FA);
  init_timer(timer_poussoir,0x7840,0x017D);
  init_timer(timer_tempo,0x4B40,0x004C);

  /* SW0 : Afficher 24/12
  SW1 : Afficher alarme
  SW8 : Alarme ON/OFF
  SW9 : Réglage heure actuelle
  SW9 + SW1 : Réglage heure alarme
  */
  while (1)
  {
    // Compteur
    compteur_secondes();

    // On regarde l'état des interrupteurs SW0 et SW1 seulement
    switch((IORD(INTERRUPTEURS_BASE,0) & 11))
    {
      /* Affichage de l'heure actuelle format 24h */
      case (0x0) :
      // Si SW9 est à l'état haut : commence le réglage
      if (((IORD(INTERRUPTEURS_BASE,0) & 0x200)>>9) == 1)
      {
        reg_heure = heure;
        reg_minutes = minutes;
        while (((IORD(INTERRUPTEURS_BASE,0) & 1000000000) >> 9)==1)
        {
        	compteur_secondes();
            reglages();
            aff_reglages();
        }
        compteur = reg_heure*3600 + reg_minutes*60 + compteur%60;
      }
      // Si SW9 est à l'état bas : on affiche juste l'heure actuelle
      else afficheur_24h();
      break;

      /* Affichage de l'heure actuelle format 12h */
      case (0x1) :
      if (((IORD(INTERRUPTEURS_BASE,0) & 0x200)>>9) == 1)
      {
        reg_heure = heure;
        reg_minutes = minutes;
        while (((IORD(INTERRUPTEURS_BASE,0) & 1000000000) >> 9)==1)
        {
        	compteur_secondes();
            reglages();
            aff_reglages();
        }
        compteur = reg_heure*3600 + reg_minutes*60 + compteur%60;
      }
      else afficheur_12h();
      break;

      /* Affichage ou réglage de l'heure d'alarme en format 24h */
      case (0x2) :
      if (((IORD(INTERRUPTEURS_BASE,0) & 0x200)>>9) == 1)
      {
        reg_heure = alarme_heure;
        reg_minutes = alarme_minutes;
        while (((IORD(INTERRUPTEURS_BASE,0) & 1000000000) >> 9)==1)
        {
        	compteur_secondes();
            reglages();
            aff_reglages();
        }
        alarme_minutes = reg_minutes;
        alarme_heure = reg_heure;
      }
      else afficheur_alarme();
      break;

      /* Affichage ou réglage de l'heure d'alarme en format 12h */
      case (0x3) :
      if (((IORD(INTERRUPTEURS_BASE,0) & 0x200)>>9) == 1)
      {
        reg_heure = alarme_heure;
        reg_minutes = alarme_minutes;
        while (((IORD(INTERRUPTEURS_BASE,0) & 1000000000) >> 9)==1)
        {
        	compteur_secondes();
            reglages();
            aff_reglages();
        }
        alarme_minutes = reg_minutes;
        alarme_heure = reg_heure;
      }
      else afficheur_alarme();
      break;
    }

    /* Partie activation de l'alarme */
    // Si SW8 est actif alors on allume tous les LEDs, sinon on éteint tout
    if (((IORD(INTERRUPTEURS_BASE,0) & 0x100)>>8) == 1)
    {
      IOWR(LEDR_BASE,0,0x3FF);
    }
    else IOWR(LEDR_BASE,0,0x0);

	/* Choix de la mélodie :
	 * Le switch 7 position haute : réglage mélodie
	 * Appui sur le PB1 : Mélodie++
	 * Appui sur le PB2 : Mélodie--
	 * Le switch 7 position basse : fin réglage mélodie
	 */
    if (((IORD(INTERRUPTEURS_BASE,0) & 0x80)>>7)==1)
    {
    	while(((IORD(INTERRUPTEURS_BASE,0) & 0x80)>>7)==1)
    	{
        	compteur_secondes();
            reg_melodie();
            aff_melodie();
    	}
    }

    /* Lorsque l'heure actuelle atteint l'heure d'alarme et SW8 est actif, une mélodie joue
    Elle s'éteint automatiquement après 1 minute ou si SW8 est inactif */
    while ((((IORD(INTERRUPTEURS_BASE,0) & 0x100)>>8) == 1) && (alarme_heure == heure && alarme_minutes == minutes))
    {
    	compteur_secondes();
    	/* Snooze 5 minutes : push button 1 ou 2 */
    	snooze();

    	/* Mélodies */

    	if (melodie == 0)
    	{
    		melodie_1();
    	}
    	if (melodie == 1)
    	{
    		melodie_2();
    	}
    	if (melodie == 2)
    	{
    		melodie_3();
    	}
    }
  } // fin while
}// fin boucle main

