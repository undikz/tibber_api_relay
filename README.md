# tibber_api_relay

Skrevet for NodeMCU/ESP8266.

Denne koden henter morgendagens priser fra tibber sitt API, regner ut gjennomsnittsprisen for dagen og lukker en releutgang (PIN 3 blir HØY) basert på om prisen overstiger X antall øre over snitt i en eller flere timer i løpet av dagen. 
Du kan se resultatet i serial output eller på en nettside publisert på samme IP som ESP8266.
Pristerskelen for aktivering er på 10øre (0.10kr) pr. kWh. Pristerskelen kan endres via nettsiden.
Sjekk at SHA1 fingeravtrykket er riktig ifht. Tibber sin API-host hvis du får 400-feil. API-Kallet skjer klokken 13.50 hver dag. (morgendagens strømpriser blir publisert ca. 13.00). Jeg har kommentert ut en del prints til serial jeg har brukt til debugging, men jeg har ikke fjernet de i tilfelle noen andre får bruk for de.               
