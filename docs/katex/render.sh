#!/usr/bin/env sh

for file in docs/katex/math/inline/*.katex; do
  echo $file
  npx --yes katex --input "$file" | tr -d '\n' > "$file.html"
done

for file in docs/katex/math/display/*.katex; do
  echo $file
  npx --yes katex --display-mode --input "$file" | tr -d '\n' > "$file.html"
done