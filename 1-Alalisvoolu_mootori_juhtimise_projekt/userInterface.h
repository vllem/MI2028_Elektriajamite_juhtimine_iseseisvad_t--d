/*
  userInterface.h
  Käsukäsitlus: Serial monitorist käskude tõlgendamine ja täitmine.

  MI.2028 Elektriajamite juhtimine
  Autor: Villem Haug                                                                                                                          
  2026/2027 sügissemester   
*/

#ifndef COMMANDS_H
#define COMMANDS_H

#include <Arduino.h>
#include "motorDriver.h"

/*
  'commandHandler' on viit funktsioonile, mis võtab mootori ja käsu argumendi.
  Kõik käsufunktsioonid peavad olema sellise kujuga, et neid saaks hoida
  ühes tabelis ja välja kutsuda ühtemoodi.
*/

typedef void (*commandHandler)(struct motorFactory &motor, const String &arg);


//  Struct 'command' paneb paika käskude struktuuri.
struct command {
  const char *name;         // Käsunimetus
  commandHandler handler;   // Funktsioon, mis täidab käsu
  const char *usage;        // Kuidas kasutada käsku
  const char *description;  // Käsu enda kirjeldus
};

// handleCommand võtab sisendrea (nt "speed 150") ja käivitab vastava käsu.
void handleCommand(struct motorFactory &motor, String input);

// Käsufunktsioonide eeldeklaratsioonid, et neid saaks kasutada tabelis enne nende definitsiooni
static void cmdStart(struct motorFactory &motor, const String &arg);
static void cmdStop(struct motorFactory &motor, const String &arg);
static void cmdSpeed(struct motorFactory &motor, const String &arg);
static void cmdHelp(struct motorFactory &motor, const String &arg);

#endif // COMMANDS_H
