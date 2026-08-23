package main

import (
	"os"
	"path/filepath"
	"strconv"
	"strings"
	"testing"
)

func writeTestFile(t *testing.T, path, content string) {
	t.Helper()
	if err := os.WriteFile(path, []byte(content), 0o644); err != nil {
		t.Fatal(err)
	}
}

func TestArticleGenerationAndFreshnessCheck(t *testing.T) {
	root := t.TempDir()
	assetPath := filepath.Join(root, "asset.svg")
	templatePath := filepath.Join(root, "index.template.html")
	outputPath := filepath.Join(root, "index.html")

	writeTestFile(t, assetPath, "<svg id=\"first\"></svg>\n")
	source := `{{ define "reference" }}{{ $reference := reference . }}[{{ $reference.Index }}]{{ end }}
Last updated on {{ date }}
{{ inline ` + strconv.Quote(assetPath) + ` }}
{{ $figure := figure "array" }}Figure {{ $figure.Index }} {{ $figure.URL }}
{{ template "reference" "2023_princeton" }}
{{ range references }}Reference {{ .Index }} {{ .Title }}{{ end }}
`
	writeTestFile(t, templatePath, source)

	if err := generateArticle(templatePath, outputPath, "1 Jan 2000"); err != nil {
		t.Fatal(err)
	}
	content, err := os.ReadFile(outputPath)
	if err != nil {
		t.Fatal(err)
	}
	rendered := string(content)
	for _, expected := range []string{
		"Last updated on 1 Jan 2000",
		"<svg id=\"first\"></svg>",
		"Figure 1 docs/plots/figures/array.svg",
		"[1]",
		"Reference 1 COS226: Data Structures and Algorithms",
	} {
		if !strings.Contains(rendered, expected) {
			t.Fatalf("generated article is missing %q", expected)
		}
	}

	if err := checkArticle(templatePath, outputPath, ""); err != nil {
		t.Fatal(err)
	}

	writeTestFile(t, assetPath, "<svg id=\"second\"></svg>\n")
	err = checkArticle(templatePath, outputPath, "")
	if err == nil || !strings.Contains(err.Error(), "is stale") {
		t.Fatalf("freshness check error = %v, want stale output", err)
	}
}

func TestRenderedArticleDateValidation(t *testing.T) {
	date, err := renderedArticleDate(
		[]byte("<small>Last updated on 28 Jul 2026\n</small>"))
	if err != nil {
		t.Fatal(err)
	}
	if date != "28 Jul 2026" {
		t.Fatalf("date = %q, want %q", date, "28 Jul 2026")
	}

	for _, content := range []string{
		"<small>no update marker</small>",
		"<small>Last updated on \n</small>",
	} {
		if _, err := renderedArticleDate([]byte(content)); err == nil {
			t.Fatalf("renderedArticleDate(%q) succeeded", content)
		}
	}
}
