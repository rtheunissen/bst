# Getting attention for this project

## Core insight

The article (`index.html`) is the product, not the code. The writing is what resonates
with people — the code is evidence supporting it. Lead with the article everywhere.

## Suggestions

### 1. Focused blog post with one sharp claim

The comprehensive article covers everything, which dilutes the hook. A standalone post
with a single provocative thesis and 3–4 key charts would travel much further.

Working title: *"Red-black trees are taught everywhere but rarely the best choice —
here's what the data says"*

Outline:
- The conventional wisdom ("use AVL for reads, red-black for writes") is a misconception
- Logarithmic weight-balanced trees beat both in practice across 5 access patterns
- Show the average path length chart, the duration chart, and the rotation count
- Link to the full article for the comprehensive treatment

Post to: Hacker News, lobste.rs, r/programming, r/compsci.
A second HN submission with a sharper angle and the C benchmark results is fair game.

### 2. Browser-accessible animations

The terminal animations are impressive but high-friction — clone, build, run. Options:
- Finish the WASM build (scaffolding already exists in `main/animate/wasm/`)
- Record screen captures as GIFs or short videos embedded in the article
- Either approach lets people *see* trees growing under different strategies without
  building anything

Visuals are what get shared. A 15-second GIF of an AVL tree vs a splay tree growing
under Zipf access would be immediately compelling.

### 3. The C translation as its own story

"We translated 25 BST implementations from Go to C to eliminate GC noise from
benchmarks — here's what we found" is interesting to the systems programming crowd
independently.

Angles:
- 363,600 tests producing identical output across both languages
- 1.26x average speedup, up to 1.75x on allocation-heavy trees
- Faithful line-by-line translation preserving style and structure
- The arena allocator approach vs Go's GC

Audience: systems programmers, language comparison enthusiasts, C community.

### 4. CS education venue

The pedagogical argument (teach LBST instead of red-black) is genuinely novel.
The article is already structured like a paper — abstract, methodology, results,
conclusion.

Venues:
- SIGCSE (ACM's CS education conference)
- ACM Inroads (journal)
- A short paper distilling the key teaching contribution

The proposed ordering — (i) global balance via partition, (ii) partial rebuilding,
(iii) incremental rotations — is a concrete curriculum contribution.

### 5. arXiv preprint

The empirical comparison across 25 trees, 5 distributions, with both timing and
structural measurements is more comprehensive than most published BST benchmarking
papers.

- Submit to arXiv under cs.DS (Data Structures and Algorithms)
- Does not need peer review to be cited
- Shows up in literature searches permanently
- Gives the work a citable reference

### 6. Algorithm visualization community

Reach out directly to people who make algorithm teaching content:
- VisuAlgo (Steven Halim, NUS)
- Red Blob Games (Amit Patel)
- Algorithm Visualizer project
- CS educators on Twitter/Mastodon who share teaching resources

These communities care deeply about interactive explanations and would likely
link to or reference the work.

## Priority order

1. **Focused blog post** — immediate reach, low effort
2. **Browser animations** — shareability, medium effort
3. **arXiv preprint** — long-term discoverability, medium effort
4. **C translation post** — second wave of attention, low effort
5. **CS education venue** — academic impact, higher effort
6. **Community outreach** — ongoing, low effort per contact
