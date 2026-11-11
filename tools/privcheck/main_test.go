package main

import "testing"

func TestCheck(t *testing.T) {
	hits := []string{
		`host = "192.168.178.20"`,
		`broker 10.1.2.3`,
		`mqtt://nas.fritz.box`,
		`kontakt: jemand@beispiel.de`,
		`token ghp_abcdefghijklmnopqrstuvwxyz0123`,
		`-----BEGIN OPENSSH PRIVATE KEY-----`,
		`#define WIFI_PASS = "geheim123"`,
	}
	for _, s := range hits {
		if len(check(s)) == 0 {
			t.Errorf("nicht erkannt: %q", s)
		}
	}
	clean := []string{
		`Doku-Adresse 192.0.2.10`,
		`41898282+github-actions[bot]@users.noreply.github.com`,
		`user@example.com`,
		`$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M`,
		`int32_t lat; /* 1e-7 Grad */`,
		`host 192.168.1.1 privcheck:ok`,
	}
	for _, s := range clean {
		if w := check(s); len(w) != 0 {
			t.Errorf("Fehlalarm bei %q: %v", s, w)
		}
	}
}
