#!/bin/sh
# Usage: scripts/metrics.sh results.csv
# CSV columns (header in first line): test,actual,estimated[,anything else]
# Rows with actual = 0 are empty-room baselines and are reported separately.
[ -f "$1" ] || { echo "usage: $0 results.csv" >&2; exit 2; }
awk -F, '
NR == 1 { next }
$2 == 0 && $3 != "" { bn++; bsum += $3; next }
$2 > 0 && $3 != "" {
    n++; a = $2; e = $3; d = e - a; ad = (d < 0) ? -d : d
    sa += a; se += e; sd += d; sae += ad; spe += ad / a * 100
}
END {
    if (n == 0 && bn == 0) { print "no data rows"; exit 1 }
    if (n > 0) {
        printf "tests (people present)      : %d\n", n
        printf "mean actual / mean estimated: %.2f / %.2f\n", sa / n, se / n
        printf "mean signed error (bias)    : %+.2f people\n", sd / n
        printf "MAE                         : %.2f people\n", sae / n
        printf "MAPE                        : %.2f %%\n", spe / n
        printf "mean accuracy (100 - MAPE)  : %.2f %%\n", 100 - spe / n
        printf "devices per person (sum/sum): %.2f\n", se / sa
    }
    if (bn > 0)
        printf "empty-room baseline         : %d runs, mean estimate %.2f (should be 0)\n", bn, bsum / bn
}' "$1"
