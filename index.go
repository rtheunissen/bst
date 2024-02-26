package main

import (
   "github.com/rtheunissen/bst/utility"
   "golang.org/x/exp/slices"
   "os"
   "text/template"
   "time"
)

type Reference struct {
   Authors   string
   Title     string
   Specifics string
   Year      int
   URL       string
   Index     int
}

var references []Reference

var References = map[string]Reference{
   "1973_knuth_taocp_vol_3_section_6": {
      Authors: "Donald E. Knuth",
      Title:   "The Art of Computer Programming, Vol. 3: Sorting and Searching",
      Year:    1972,
      URL:     "docs/references/1973_knuth_taocp_vol_3_section_6.pdf",
   },
   "1972_clark_allan_crane": {
      Authors: "Clark A. Crane",
      Title:   "Linear lists and priority queues as balanced binary trees",
      Year:    1972,
      URL:     "docs/references/1972_clark_allan_crane.pdf",
   },
   "1986_culberson": {
      Authors: "J. C. Culberson",
      Title:   "The effect of asymmetric deletions on binary search trees",
      Year:    1986,
      URL:     "docs/references/1986_culberson.pdf",
   },
   "1989_culberson_munro": {
      Authors: "J. C. Culberson, J. I. Munro",
      Title:   "Explaining the behavior of binary search trees under prolonged updates: a model and simulations",
      Year:    1989,
      URL:     "docs/references/1989_culberson_munro.pdf",
   },
   "1961_hibbard": {
      Authors: "Thomas N. Hibbard",
      Title:   "Some combinatorial properties of certain trees with applications to searching and sorting",
      Year:    1961,
      URL:     "docs/references/1961_hibbard.pdf",
   },
   "1983_eppinger": {
      Authors: "Jeffrey L. Eppinger",
      Title:   "An empirical study of insertion and deletion in binary search trees",
      Year:    1983,
      URL:     "docs/references/1983_eppinger.pdf",
   },
   "2004_jorg": {
      Authors: "Jörg Schmücker",
      Title:   "Apache Commons Collections",
      Specifics: `"TreeList"`,
      Year: 2004,
      URL: "https://github.com/apache/commons-collections/blob/3a5c5c2838d0dacbed2722c4f860d36d0c32f325/src/main/java/org/apache/commons/collections4/list/TreeList.java",
      // https://markmail.org/message/43ux2i3rbsigtotu?q=TreeList+list:org%2Eapache%2Ecommons%2Edev/&page=4#query:TreeList%20list%3Aorg.apache.commons.dev%2F+page:4+mid:mv2nw4ajw2kywmku+state:results
   },
   "1980_stephenson": {
      Authors: "C. J. Stephenson",
      Title:   "A method for constructing binary search trees by making insertions at the root",
      Year:    1980,
      URL:     "docs/references/1980_stephenson.pdf",
   },
   "2017_muusse": {
      Authors: "Ivo Muusse",
      Title:   "An algorithm for balancing a binary search tree",
      Year:    2017,
      URL:     "docs/references/2017_muusse.pdf",
   },
   "2001_roura": {
      Authors: "Salvador Roura",
      Title:   "A new method for balancing binary search trees",
      Year:    2001,
      URL:     "docs/references/2001_roura.pdf",
   },
   "2002_chan": {
      Authors: "Timothy M. Chan",
      Title:   "Closest-point problems simplified on the RAM",
      Year:    2001,
      URL:     "docs/references/2002_chan.pdf",
   },
   "2013_warren_hackers_delight": {
      Authors:   "Henry S. Warren, Jr.",
      Title:     "Hacker's Delight",
      Specifics: "Second Edition, Ch. 5, page 106",
      Year:      2013,
      URL:       "docs/references/2012_warren_hackers_delight.pdf",
   },
   "2000_cho_sahni": {
      Authors: "Seonghun Cho, Sartaj Sahni",
      Title:   "A new weight balanced binary search tree",
      Year:    2000,
      URL:     "docs/references/2000_cho_sahni.pdf",
   },
   "1962_avl": {
      Authors: "G. M. Adelson-Velsky, E. M. Landis",
      Title:   "An algorithm for the organization of information",
      Year:    1962,
      URL:     "docs/references/1962_avl.pdf",
   },
   "1978_guibas_sedgewick": {
      Authors: "Leo J. Guibas, Robert Sedgewick",
      Title:   "A dichromatic framework for balanced trees",
      Year:    1978,
      URL:     "docs/references/1978_guibas_sedgewick.pdf",
   },
   "2013_rank_balanced_trees": {
      Authors: "Bernhard Haeupler, Siddhartha Sen, Robert E. Tarjan",
      Title:   "Rank-balanced trees",
      Year:    2013,
      URL:     "docs/references/2013_rank_balanced_trees.pdf",
   },
   "2018_ravl": {
      Authors: "Siddhartha Sen, Robert E. Tarjan, David Hong Kyun Kim",
      Title:   "Deletion without rebalancing in binary search trees",
      Year:    2018,
      URL:     "docs/references/2018_ravl.pdf",
   },
   "2015_conc": {
      Authors: "Aleksandar Prokopec, Martin Odersky",
      Title:   "Conc-trees for functional and parallel programming",
      Year:    2015,
      URL:     "docs/references/2015_conc.pdf",
   },
   "1972_nievergelt_reingold": {
      Authors: "J. Nievergelt, E. M. Reingold",
      Title:   "Binary search trees of bounded balance",
      Year:    1972,
      URL:     "docs/references/1972_nievergelt_reingold.pdf",
   },
   "1980_blum_mehlhorn": {
      Authors: "N. Blum, K. Mehlhorn",
      Title:   "On the average number of rebalancing operations in weight-balanced trees",
      Year:    1980,
      URL:     "docs/references/1980_blum_mehlhorn.pdf",
   },
   "1993_lai_wood": {
      Authors: "Tony W. Lai, Derick Wood",
      Title:   "A top-down updating algorithm for weight-balanced trees",
      Year:    1993,
      URL:     "docs/references/1993_lai_wood.pdf",
   },
   "2005_frias": {
      Authors: "L. Frias",
      Title:   "Extending STL maps using LBSTs",
      Year:    2005,
      URL:     "docs/references/2005_frias.pdf",
   },
   "2011_hirai_yamamoto": {
      Authors: "Yoichi Hirai, Kazuhiko Yamamoto",
      Title:   "Balancing weight-balanced trees",
      Year:    2011,
      URL:     "docs/references/2011_hirai_yamamoto.pdf",
   },
   "2019_barth_wagner": {
      Authors: "Lukas Barth, Dorothea Wagner",
      Title:   "Engineering top-down weight-balanced trees",
      Year:    2019,
      URL:     "docs/references/2019_barth_wagner.pdf",
   },
   "1986_dsw": {
      Authors: "Quentin F. Stout, Bette L. Warren",
      Title:   "Tree rebalancing in optimal time and space",
      Year:    1986,
      URL:     "docs/references/1986_dsw.pdf",
   },
   "1976_day": {
      Authors: "A. Colin Day",
      Title:   "Balancing a binary tree",
      Year:    1976,
      URL:     "docs/references/1976_day.pdf",
   },
   "1983_overmars": {
      Authors: "M. Overmars",
      Title:   "The Design of Dynamic Data Structures",
      Year:    1983,
      URL:     "docs/references/1983_overmars.pdf",
   },
   "1989_andersson": {
      Authors: "A. Andersson",
      Title:   "Improving partial rebuilding by using simple balance criteria",
      Year:    1989,
      URL:     "docs/references/1989_andersson.pdf",
   },
   "1999_andersson": {
      Authors: "A. Andersson",
      Title:   "General balance trees",
      Year:    1999,
      URL:     "docs/references/1999_andersson.pdf",
   },
   "1993_galperin_rivest": {
      Authors: "Igal Galperin, Ronald L. Rivest",
      Title:   "Scapegoat trees",
      Year:    1993,
      URL:     "docs/references/1993_galperin_rivest.pdf",
   },
   "1996_seidel_aragon": {
      Authors: "Raimund Seidel, Cecilia R. Aragon",
      Title:   "Randomized search trees",
      Year:    1996,
      URL:     "docs/references/1996_seidel_aragon.pdf",
   },
   "2012_bagwell_rompf": {
      Authors: "Phil Bagwell, Tiark Rompf",
      Title:   "RRB-trees: efficient immutable vectors",
      Year:    2012,
      URL:     "docs/references/2012_bagwell_rompf.pdf",
   },
   "2015_stucki_rompf_ureche_bagwell": {
      Authors: "Nicolas Stucki, Tiark Rompf, Vlad Ureche, Phil Bagwell",
      Title:   "RRB vector: a practical general purpose immutable sequence",
      Year:    2015,
      URL:     "docs/references/2015_stucki_rompf_ureche_bagwell.pdf",
   },
   "2015_stucki": {
      Authors: "Nicolas Stucki",
      Title:   "Turning Relaxed Radix Balanced Vector from Theory into Practice for Scala Collections",
      Year:    2015,
      URL:     "docs/references/2015_stucki.pdf",
   },
   "1989_pugh": {
      Authors: "William Pugh",
      Title:   "Skip Lists: A Probabilistic Alternative to Balanced Trees",
      Year:    1989,
      URL:     "docs/references/1989_pugh.pdf",
   },
   "2001_blandford_blelloch": {
      Authors: "Dan Blandford, Guy Blelloch",
      Title:   "Functional Set Operations with Treaps",
      Year:    2001,
      URL:     "docs/references/2001_blandford_blelloch.pdf",
   },
   "2006_rodeh": {
      Authors: "Ohad Rodeh",
      Title:   "B-trees, Shadowing, and Clones",
      Year:    2006,
      URL:     "docs/references/2006_rodeh.pdf",
   },
   "2017_puente": {
      Authors: "Juan Pedro Bolívar Puente",
      Title:   "Persistence for the masses: RRB-vectors in a systems language",
      Year:    2017,
      URL:     "docs/references/2017_puente.pdf",
   },
   "1988_tarjan_van_wyk": {
      Authors: "Robert E. Tarjan, Christopher J. van Wyk",
      Title:   "An O(n log log n)-time algorithm for triangulating a simple polygon",
      Year:    1988,
      URL:     "docs/references/1988_tarjan_van_wyk.pdf",
   },
   "2018_tarjan_levy_timmel": {
      Authors: "Robert E. Tarjan, Caleb C. Levy, Stephen Timmel",
      Title:   "Zip trees",
      Year:    2018,
      URL:     "docs/references/2018_tarjan_levy_timmel.pdf",
   },
   "1997_martinez_roura": {
      Authors: "Conrado Martínez, Salvador Roura",
      Title:   "Randomized Binary Search Trees",
      Year:    1997,
      URL:     "docs/references/1997_martinez_roura.pdf",
   },
   "2022_clrs_delete": {
      Authors: "Thomas H. Cormen, Charles E. Leiserson, Ronald L. Rivest, Clifford Stein",
      Title:   "Introduction to Algorithms",
      Year:    2022,
      URL:     "docs/references/2022_clrs_delete.pdf",
   },
   "2013_goodrich_tamassia_goldwasser_delete": {
      Authors: "Michael T. Goodrich, Roberto Tamassia, Michael H. Goldwasser",
      Title:   "Data Structures and Algorithms in Python",
      Year:    2013,
      URL:     "docs/references/2013_goodrich_tamassia_goldwasser_delete.pdf",
   },
   "2011_sedgewick_wayne_delete": {
      Authors: "Robert Sedgewick, Kevin Wayne",
      Title:   "Algorithms",
      Specifics: "4th Edition, Chapter 3, Page 410",
      Year:    2013,
      URL:     "docs/references/2011_sedgewick_wayne_delete.pdf",
   },
   "2013_drozdek_delete": {
      Authors:   "Adam Drozdek",
      Title:     "Data Structures and Algorithms in C++",
      Specifics: "4th Edition, Section 6.6, Pages 243-249",
      Year:      2013,
      URL:       "docs/references/2013_drozdek_delete.pdf",
   },
   "2008_skiena_delete": {
      Authors: "Steven S. Skiena",
      Title:   "The Algorithm Design Manual",
      Specifics: "2nd Edition, Chapter 3.4, Page 81",
      Year:    2008,
      URL:     "docs/references/2008_skiena_delete.pdf",
   },
   "2014_weiss_delete": {
      Authors: "Mark Allen Weiss",
      Title:   "Data Structures and Algorithm Analysis in C++",
      Specifics: "4th Edition, Chapter 4.3.4, Page 139",
      Year:    2014,
      URL:     "docs/references/2014_weiss_delete.pdf",
   },
   "1989_manber": {
      Authors: "Udi Manber",
      Title:   "Introduction to Algorithms: A Creative Approach",
      Specifics: "Chapter 4.3, Page 73",
      Year:    1989,
      URL:     "docs/references/1989_manber.pdf",
   },
   "1972_bayer_mccreight": {
      Authors: "R. Bayer, E. McCreight",
      Title: "Organization and maintenance of large ordered indexes",
      Year: 1972,
      URL: "docs/references/1972_bayer_mccreight.pdf",
   },
   "2023_princeton": {
      Year: 2023,
      Authors: "Princeton",
      Title: "COS226: Data Structures and Algorithms",
      URL: "http://web.archive.org/web/20230812194316/https://www.cs.princeton.edu/courses/archive/spring23/cos226/lectures.php",
   },
   "2023_stanford": {
      Year: 2023,
      Authors: "Stanford",
      Title: "CS166: Data Structures",
      URL: "http://web.archive.org/web/20230812194631/http://web.stanford.edu/class/cs166/",
   },
   "2023_uc_berkeley": {
      Year: 2023,
      Authors: "UC Berkeley",
      Title: "CS 61B: Data Structures",
      URL: "http://web.archive.org/web/20230812195004/https://sp23.datastructur.es/",
   },
   "redblack_java_treemap": {
      Year: 1997,
      Title: "java.util.TreeMap",
      Authors: "Josh Bloch, Doug Lea",
      URL: "https://web.archive.org/web/20230812200354/https://docs.oracle.com/en/java/javase/20/docs/api/java.base/java/util/TreeMap.html",
   },
   "redblack_linux_kernel": {
      Year: 1999,
      Title: "include/linux/rbtree.h",
      Authors: "Andrea Arcangeli, Linus Torvalds",
      URL: "https://github.com/torvalds/linux/blob/4815a36009044ba69a9b8d781943ec6505c451a2/include/linux/rbtree.h",
   },
   "1994_splay_with_size": {
      Year: 1994,
      Title: "An implementation of top-down splaying with sizes",
      Authors: "D. Sleator",
      URL: "https://www.link.cs.cmu.edu/link/ftp-site/splaying/top-down-size-splay.c",
   },
   "2009_redis": {
      Year: 2009,
      Title: "Redis ZSET",
      Authors: "Salvatore Sanfilippo",
      URL: "https://github.com/redis/redis/blob/9b1d4f003de1b141ea850f01e7104e7e5c670620/src/server.h#L1335",
   },
   "2011_rocksdb": {
      Year: 2011,
      Title: "RocksDB",
      Authors: "Jeffrey Dean, Sanjay Ghemawat",
      URL: "https://github.com/facebook/rocksdb/blob/c3c84b3397a0eaa6450340ecea3b267c0e6c1f3c/memtable/skiplist.h#L46",
   },
   "2007_dean_jones": {
      Year: 2007,
      Authors: "Brian C. Dean, Zachary H. Jones",
      Title: "Exploring the duality between skip lists and binary search trees",
      URL: "docs/references/2007_dean_jones.pdf",
   },
}

func reference(key string) Reference {
   if _, defined := References[key]; !defined {
      panic("reference not defined: " + key)
   }
   for _, reference := range references {
      if reference.Title == References[key].Title {
         return reference
      }
   }
   reference := References[key]
   reference.Index = len(references) + 1
   references = append(references, reference)
   return reference
}

type Figure struct {
   Key string
   URL string
   Index int
}

var figures []Figure

var Figures = []Figure {
   {
      Key: "delete",
      URL: "docs/plots/figures/delete/delete.svg",
   },
   {
      Key: "polytope",
      URL: "docs/plots/figures/polytope/polytope.svg",
   },
   {
      Key: "leaf",
      URL: "docs/plots/figures/leaf.svg",
   },
   {
      Key: "tree",
      URL: "docs/plots/figures/tree.svg",
   },
   {
      Key: "split_join",
      URL: "docs/plots/figures/split_join.svg",
   },
   {
      Key: "finger_tree",
      URL: "docs/plots/figures/finger_tree.svg",
   },
   {
      Key: "rotations",
      URL: "docs/plots/figures/rotations.svg",
   },
   {
      Key: "linked_list",
      URL: "docs/plots/figures/linked_list.svg",
   },
   {
      Key: "linked_list_median",
      URL: "docs/plots/figures/linked_list_median.svg",
   },
   {
      Key: "partition",
      URL: "docs/plots/figures/partition.svg",
   },
   {
      Key: "binary_search_tree",
      URL: "docs/plots/figures/binary_search_tree.svg",
   },
   {
      Key: "binary_search_tree_large",
      URL: "docs/plots/figures/binary_search_tree_large.svg",
   },
   {
      Key: "hibbard",
      URL: "docs/plots/figures/hibbard.svg",
   },
   {
      Key: "balance",
      URL: "docs/plots/figures/balance.svg",
   },
   {
      Key: "scapegoat",
      URL: "docs/plots/figures/scapegoat.svg",
   },
   {
      Key: "persistence",
      URL: "docs/plots/figures/persistence.svg",
   },
   {
      Key: "parent_pointers",
      URL: "docs/plots/figures/parent_pointers.svg",
   },
   {
      Key: "concurrency",
      URL: "docs/plots/figures/concurrency.svg",
   },
   {
      Key: "median_balance",
      URL: "docs/plots/figures/median_balance.svg",
   },
   {
      Key: "perfect_trees",
      URL: "docs/plots/figures/perfect_trees.svg",
   },
   {
      Key: "redblack",
      URL: "docs/plots/figures/redblack.svg",
   },
   {
      Key: "array",
      URL: "docs/plots/figures/array.svg",
   },
   {
      Key: "insert_leaf",
      URL: "docs/plots/figures/insert_leaf.svg",
   },
   {
      Key: "log_balanced",
      URL: "docs/plots/figures/log_balanced.svg",
   },
   {
      Key: "mlogm",
      URL: "docs/plots/figures/mlogm.svg",
   },
}

func figure(key string) Figure {
   for _, figure := range figures {
      if figure.Key == key {
         return figure
      }
   }
   index := slices.IndexFunc(Figures, func(figure Figure) bool {
      return figure.Key == key
   })
   if index < 0 {
      panic("figure not defined: " + key)
   }
   figure := Figures[index]
   figure.Index = len(figures) + 1
   figures = append(figures, figure)
   return figure
}

var funcMap = template.FuncMap{
   "inline": func(path string) (string, error) {
      content, err := os.ReadFile(path)
      return string(content), err
   },
   "add": func(n, i int) int {
      return n + i
   },
   "date": func() (string, error) {
      return time.Now().Format("_2 Jan 2006"), nil
   },
   "reference": func(key string) Reference{
      return reference(key)
   },
   "references": func() []Reference {
      return references
   },
   "figure": func(key string) Figure {
      return figure(key)
   },
}

func Index(file string, tmpl *template.Template) {
   if err := tmpl.Execute(utility.Must(os.Create(file)), nil); err != nil {
      panic(err)
   }
}

func Template(path string) *template.Template {
   tmpl := string(utility.Must(os.ReadFile(path)))
   return utility.Must(template.New("index").Funcs(funcMap).Parse(tmpl))
}

func main() {
   Index("index.html", Template("index.template.html"))
}
