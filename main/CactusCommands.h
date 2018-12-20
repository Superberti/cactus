#ifndef COCOAANDWINDOXTYPES_H
#define COCOAANDWINDOXTYPES_H

// Maximale Zeilenlaenge einer Kommandozeile Server->Client 
#define MAX_LINE_LENGTH 16384

/// Vereinbarungen fuer den Rueckgabewert eines Client-Kommandos
#define CLIENT_OK       "OK"        ///< Kommando erfolgreich
#define CLIENT_ERR      "ERR"       ///< Fehler im Kommando aufgetreten
#define CLIENT_PENDING  "PENDING"   ///< Befehl dauert laenger als der TCP/IP-Timeout, frag spaeter noch mal nach...

/// Status bei einem Kommandothread
#define THREAD_OK					"THREAD_OK"						///< Thread wurde erfolgreich ausgefuehrt
#define THREAD_ERR				"THREAD_ERR"					///< Thread wurde mit Fehler abgebrochen
#define THREAD_PENDING		"THREAD_PENDING"			///< Thread laeuft gerade
#define THREAD_NOTEXISTS	"THREAD_NOTEXISTS"		///< Momentan existiert gar kein Thread

/// Fuer Binaeuebertragungen CLIENT_OK vorangestellt:
/// Beispiel: "OK BINDATA 64"  -> nach dem return kommen 64 Bytes Daten
#define CLIENT_BINDATA  "BINDATA"   ///< Es kommen nach einem return Binaerwerte
#define CLIENT_BASE64   "BASE64"    ///< Es kommen Base64-kodierte Werte


/// Kommandos

/// Alle Kommandos sowie alle Parameter sind in Klartext als Strings gehalten.
/// Einzige Ausnahme ist hierbei die Antwort beim Lesen vom Server mit CLIENT_BINDATA
/// Nach diesem Konstrukt muessen die darauffolgenden Daten binaer gelesen werden.
/// Ist alles in Ordnung, so antwortet der Server mit: OK Rueckgabeparameter1..2..n.
/// Bei einem Fehler antwortet der Server mit ERR Fehlernummer Klartext.
/// Ein Clientkommando beginnt immer mit dem in CocoaCommands[] definierten Kommandostrings
/// und den Parametern, die mit Leerzeichen getrennt werden. Gibt es in einem Parameter
/// Leerzeichen, so ist der Parameter in Anfuehrungszeichen zu setzen.
/// Alle Befehle und Antworten muessen in einer Zeile stehen und enden mit einem Return.
enum TCocoaCommand {
	/// Unbekanntes Kommando
	eCMD_UNKNOWN 						= 0,
 
	/// Name des Clients setzen
	/// Parameter: Name des Clients
	/// Rueckgabe: OK
	eCCD_CLIENTNAME   			= 1,
 
	/// Version des Clients setzen
	/// Parameter: Version des Clients
	/// Rueckgabe: OK
	eCMD_CLIENTVERSION   		= 2,
 
	/// Eindeutige ID des Clients setzen
	/// Parameter: ID des Clients (eindeutige Prozess-ID)
	/// Rueckgabe: OK
	eCMD_CLIENTID     			= 3,
 
	/// Hostname des Clients setzen
	/// Parameter: Hostname des Clients
	/// Rueckgabe: OK
	eCMD_CLIENTHOSTNAME			= 4,
 
	/// Version des Servers abfragen	
	/// Parameter: keine
	/// Rueckgabe: OK Serverversion
	eCMD_GETSERVERVERSION		= 5,
 
	/// Hallo-Testkommando
	/// Parameter: keine
	/// Rueckgabe: OK
	eCMD_HELLO							= 6,
 
	/// Testkommando. Liefert OK und die Eingabeparameter zurueck
	/// Parameter: keine
	/// Rueckgabe: OK Eingabeparameter
	eCMD_TESTCMD						= 7,
 
	/// Fordert vom Server eine Authentifizierungs-ID an
	/// Parameter: keine
	/// Rueckgabe: OK ID
	eCMD_GETAUTHCODE				= 8,
 
	/// Client uebermittelt die verschluesselte Auth.-ID
	/// Parameter: Verschluesselte Antwort
	/// Rueckgabe: OK
	eCMD_SETAUTHRESPONSE		= 9,
  
	/// Aus der Geraetekonfigurationsdatei einen Wert lesen
	/// Parameter: Pfad fuer den Parameter
	/// Knoten werden durch "/" getrennt, Parameter durch "#"
	/// Die Werte werden in einer XML-Datei gespeichert
	/// Rueckgabe: OK Iteminhalt (leer wenn Item nicht da)
	/// Beispiel: GETCONFIGITEM "setting/device#type"
	eCMD_GETCONFIGITEM			= 10,
 
	/// In die Geraetekonfigurationsdatei einen Wert schreiben
	/// Parameter: Pfad fuer den Parameter, Wert des Parameters
	/// Knoten werden durch "/" getrennt, Parameter durch "#"
	/// Die Werte werden in einer XML-Datei gespeichert
	/// Rueckgabe: OK
	/// Beispiel: SETCONFIGITEM "setting/device#comment" "Das ist ein Kommentar"
	eCMD_SETCONFIGITEM			= 11,
 
	/// Einen Wert in der Geraetekonfigurationsdatei loeschen
	/// Parameter: Pfad fuer den Parameter
	/// Rueckgabe: OK
	eCMD_DELETECONFIGITEM		=	12,
 
  /// Existiert ein bestimmter Knoten oder Attribut in der
  /// Server-Konfigurationsdatei
  /// Parameter: Pfad fuer den Parameter
	/// Rueckgabe: OK 0/1 (0=item existiert nicht, 1=item existiert)
  eCMD_CONFIGITEM_EXISTS  = 13,

  /// RGB-Wert einer spezifischen LED setzen (0-143)
  /// Parameter: LED-Nummer, R, G, B
  /// Beispiel: "SETLED 99 255 128 255"
  eCMD_SET_LED = 14,

  /// Alle LEDs setzen. Es werden 144 RGB-Werte erwartet
  /// Parameter: R0, G0, B0 ... R143, G143, B143
  eCMD_SET_LEDS = 15,

  /// Einen Bereich (Start bis Ende) von LEDs setzen
  /// Parameter: Start-LED-Nummer, End-LED-Nummer, Rs, Gs, Bs ... Re, Ge, Be
  eCMD_SET_LED_RANGE = 16,

  /// LED-Effekt abspielen
  /// Parameter: Effektnummer (0=aus)
  eCMD_LED_EFFECT = 17,

  /// LED-Füllgrad in Prozent setzen (mit Farbe)
  /// Parameter: Füllgrad in Prozent, R, G, B
  eCMD_FILL_CACTUS = 18,
	
};

/// Kommandostruktur. Bindet den Befehl an eine feste Zeichenkette
struct TCommandStruct
{
	TCocoaCommand 	mCmd;				  ///< Identifizierer des Kommandos.
	char						mCmdStr[40]; 	///< Dazugehoeriger Kommandostring
	int 						mNumParams;		///< Mindestanzahl zum Befehl zugehoeriger Parameter. Zusaetzliche Parameter, die der Server (noch) nicht versteht, werden ignoriert!
};//TWxSensorData

//----------------------------------------------------------------------------

/// Liste aller gueltigen Kommandos. Ueber dieses Feld koennen die
/// Kommandostrings direkt angesprochen werden, ohne direkt im
/// Quelltext ausgeschrieben zu werden. Damit vermeidet man
/// schwer zu findende Schreibfehler bei den Kommandostrings, die
/// dann erst im Betrieb auffallen wuerden.

const TCommandStruct CactusCommands[] = {
// Kommando									Kommandostring			Mindestzahl an Parametern   
	{eCMD_UNKNOWN,					"UNKNOWN"    				    ,0 },
	{eCMD_CLIENTVERSION,		"CLIENTVERSION"			    ,1 },
	{eCMD_CLIENTID,					"CLIENTID"					    ,1 },
	{eCMD_CLIENTHOSTNAME, 	"CLIENTHOSTNAME" 		    ,1 },
	{eCMD_GETSERVERVERSION,	"GETSERVERVERSION"	    ,0 },
	{eCMD_HELLO,						"HELLO"							    ,0 },
	{eCMD_TESTCMD,					"TESTCMD"						    ,0 },
	{eCMD_GETAUTHCODE,			"GETAUTHCODE"				    ,0 },
	{eCMD_SETAUTHRESPONSE,	"SETAUTHRESPONSE"		    ,1 },
	{eCMD_GETCONFIGITEM,		"GETCONFIGITEM"			    ,1 },
	{eCMD_SETCONFIGITEM,		"SETCONFIGITEM"			    ,1 },
  {eCMD_DELETECONFIGITEM,	"DELETECONFIGITEM"	    ,1 },
  {eCMD_CONFIGITEM_EXISTS,"CONFIGITEM_EXISTS"	    ,1 },
  {eCMD_SET_LED,          "SETLED"	              ,4 },
  {eCMD_SET_LEDS,         "SETLEDS"	              ,3 },
  {eCMD_SET_LED_RANGE,    "SETLEDRANGE"	          ,5 },
  {eCMD_LED_EFFECT,       "LEDEFFECT"	            ,1 },
  {eCMD_FILL_CACTUS,      "FILLCACTUS"	          ,4 },
};

#endif
