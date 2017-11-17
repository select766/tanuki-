set terminal png
set datafile separator ","
set grid

set output "03_mean_cross_entropy.png"
set title "mean cross entropy"
plot "loss.csv" using 1:4 with lines title "train" , \
     "loss.csv" using 1:9 with lines title "test"

set output "04_mean_cross_entropy_eval.png"
set title "mean cross entropy eval"
plot "loss.csv" using 1:5 with lines title "train" , \
     "loss.csv" using 1:10 with lines title "test"

set output "05_mean_cross_entropy_win.png"
set title "mean cross entropy win"
plot "loss.csv" using 1:6 with lines title "train" , \
     "loss.csv" using 1:11 with lines title "test"

set output "06_mean_cross_entropy_eval_win.png"
set title "mean cross entropy eval win"
plot "loss.csv" using 1:5 with lines title "train eval" , \
     "loss.csv" using 1:10 with lines title "test eval" , \
     "loss.csv" using 1:6 with lines title "train win" , \
     "loss.csv" using 1:11 with lines title "test win"

set output "07_norm.png"
set title "norm"
plot "loss.csv" using 1:12 with lines title "norm"

set output "08_mean_entropy_eval.png"
set title "mean entropy eval"
plot "loss.csv" using 1:15 with lines title "train" , \
     "loss.csv" using 1:17 with lines title "test"

set output "09_mean_kld_eval.png"
set title "mean kld eval"
plot "loss.csv" using 1:16 with lines title "train" , \
     "loss.csv" using 1:18 with lines title "test"

set output "10_mean_eval_value.png"
set title "mean eval value"
plot "loss.csv" using 1:19 with lines title "train mean eval value", \
     "loss.csv" using 1:23 with lines title "test mean eval value"

set output "11_sd_eval_value.png"
set title "sd eval value"
plot "loss.csv" using 1:20 with lines title "train sd eval value", \
     "loss.csv" using 1:24 with lines title "test sd eval value"

set output "12_mean_abs_eval_value.png"
set title "mean eval value"
plot "loss.csv" using 1:21 with lines title "train mean abs eval value", \
     "loss.csv" using 1:25 with lines title "test mean abs eval value"

set output "13_sd_abs_eval_value.png"
set title "sd eval value"
plot "loss.csv" using 1:22 with lines title "train sd abs eval value", \
     "loss.csv" using 1:26 with lines title "test sd abs eval value"
