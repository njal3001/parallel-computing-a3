This file contains instructions on how to generate performance tests.

Steps:
1. Run the commands below to generate small, medium and large network configurations.
2. Adjust by hand the number of ticks and troons per line.

### Small configuration
```python
python3 gen_test.py --stations=100 --max_popularity=50 --max_link_weight=50 --max_line_size=50 --max_line_len=70 > testcases/small.in
```

### Large configuration
```python
python3 gen_test.py --stations=10000 --max_popularity=500 --max_link_weight=500 --max_line_size=2000 --max_line_len=7000 > testcases/large.in
```