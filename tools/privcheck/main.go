// privcheck sucht in den Dateien eines Repos nach Inhalten, die in einem
// oeffentlichen Repo nichts verloren haben: private IP-Adressen, Heimnetz-
// Hostnamen, E-Mail-Adressen und typische Zugangsdaten.
//
//	go run ./tools/privcheck [verzeichnis]     (Standard: .)
//
// Exit-Code 1, wenn etwas gefunden wurde; jede Fundstelle steht als
// datei:zeile: grund auf stdout. Die Muster sind absichtlich allgemein, damit
// diese Datei selbst nichts Privates enthalten muss.
//
// Ausnahme fuer eine Zeile: Kommentar "privcheck:ok" in derselben Zeile.
package main

import (
	"bufio"
	"bytes"
	"fmt"
	"io/fs"
	"os"
	"path/filepath"
	"regexp"
	"strings"
)

type rule struct {
	why string
	re  *regexp.Regexp
}

var rules = []rule{
	{"private IPv4 (RFC 1918)", regexp.MustCompile(`\b(10\.\d{1,3}\.\d{1,3}\.\d{1,3}|192\.168\.\d{1,3}\.\d{1,3}|172\.(1[6-9]|2\d|3[01])\.\d{1,3}\.\d{1,3})\b`)},
	{"Heimnetz-Hostname", regexp.MustCompile(`(?i)\b[a-z0-9-]+\.(fritz\.box|lan|home\.arpa|internal)\b`)},
	{"E-Mail-Adresse", regexp.MustCompile(`[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Za-z]{2,}`)},
	{"GitHub-Token", regexp.MustCompile(`\b(ghp|gho|ghu|ghs|ghr|github_pat)_[A-Za-z0-9_]{20,}`)},
	{"AWS-Key", regexp.MustCompile(`\bAKIA[0-9A-Z]{16}\b`)},
	{"privater Schluessel", regexp.MustCompile(`-----BEGIN [A-Z ]*PRIVATE KEY-----`)},
	{"Passwort im Klartext", regexp.MustCompile(`(?i)\b(password|passwd|passwort|wifi_pass|wlan_pass)\s*[:=]\s*["'][^"'\s]{4,}["']`)},
}

// erlaubte E-Mail-Adressen: Platzhalter und GitHub-noreply
var okMail = regexp.MustCompile(`(?i)@(example\.(com|org|net)|users\.noreply\.github\.com|noreply\.github\.com|anthropic\.com)$`)

func main() {
	root := "."
	if len(os.Args) > 1 {
		root = os.Args[1]
	}
	n, err := scan(root, os.Stdout)
	if err != nil {
		fmt.Fprintln(os.Stderr, "privcheck:", err)
		os.Exit(2)
	}
	if n > 0 {
		fmt.Printf("privcheck: %d Fundstelle(n)\n", n)
		os.Exit(1)
	}
	fmt.Println("privcheck: nichts gefunden")
}

// scan prueft alle Textdateien unter root (ohne .git, build-Ordner und
// Binaerdateien) und gibt die Zahl der Fundstellen zurueck.
func scan(root string, out *os.File) (int, error) {
	found := 0
	err := filepath.WalkDir(root, func(p string, d fs.DirEntry, err error) error {
		if err != nil {
			return err
		}
		if d.IsDir() {
			// die eigenen Testdaten enthalten absichtlich Treffer
			if filepath.ToSlash(p) == "tools/privcheck" || strings.HasSuffix(filepath.ToSlash(p), "/tools/privcheck") {
				return filepath.SkipDir
			}
			switch d.Name() {
			case ".git", "build", "cbuild", "dist", "node_modules":
				return filepath.SkipDir
			}
			return nil
		}
		data, err := os.ReadFile(p)
		if err != nil {
			return err
		}
		if bytes.IndexByte(data, 0) >= 0 { // Binaerdatei
			return nil
		}
		sc := bufio.NewScanner(bytes.NewReader(data))
		sc.Buffer(make([]byte, 1<<20), 1<<20)
		for line := 1; sc.Scan(); line++ {
			for _, why := range check(sc.Text()) {
				fmt.Fprintf(out, "%s:%d: %s\n", p, line, why)
				found++
			}
		}
		return sc.Err()
	})
	return found, err
}

// check liefert die Gruende, aus denen eine Zeile auffaellt.
func check(s string) []string {
	if strings.Contains(s, "privcheck:ok") {
		return nil
	}
	var why []string
	for _, r := range rules {
		for _, m := range r.re.FindAllString(s, -1) {
			if r.why == "E-Mail-Adresse" && okMail.MatchString(m) {
				continue
			}
			why = append(why, r.why+": "+m)
			break
		}
	}
	return why
}
