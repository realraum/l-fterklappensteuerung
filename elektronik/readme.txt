===== µC =====

ATMega32U4
5V
16MHz


===== 230VAC Lüfter =====

wird geschalten mit: Neuhold SSR Typ ZG3NC-240B
hängt im Phasenleiter.
Datasheet: http://www.ndu.cl/pdf/0509040FT.pdf
Schaltleistung: 40A mit eingebautem Snubber. bei induktiven Lasten: 40% Strom der 40A sind ok --> passt für unseren Motor uns seiner Startlastspitzen.

SSR zieht 40mA -> schaltung für einen Transistor oder FET

Zusätzlich: Varistor für 230V parallel zum SSR

z.b.: S14K680 von Neuhold
oder notfalls: 2x S10K150 in Serie

generelle Infos:
http://www.crydom.com/en/tech/whitepapers/ac_mc_whitepaper.pdf
http://forum.arduino.cc/index.php?topic=114209.0

off leakage: 10mA = 10mA (SSR) + 0mA (Varistor)


===== 12V Getriebemotoren für Lüfterklappen =====

Wird geschalten mit PowerMosFET und Freilaufdiode.

MosFET: IPP029N06N, reicht für 5V
     (alternativ: IRLR8743pbf)

Freilaufdiode parallel zum Motor, gegen Stromrichtung, sollte für >8A dimensioniert sein
SBL10L25 od. Ähnliches



===== Gabellichtschranken =====

20kOhm(?) an K Ausgang um Widerstand der Photodiode zu balancieren.
