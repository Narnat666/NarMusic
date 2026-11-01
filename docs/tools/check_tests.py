"""汇总 testDebugUnitTest 的统计结果。

用法：python docs/tools/check_tests.py [测试结果目录]
默认读取工程根下的 app/build/test-results/testDebugUnitTest。
"""
import glob
import os
import sys
import xml.etree.ElementTree as ET

DEFAULT = os.path.join(
    os.path.dirname(os.path.abspath(__file__)),
    '..', '..', 'app', 'build', 'test-results', 'testDebugUnitTest'
)
pattern = sys.argv[1] if len(sys.argv) > 1 else DEFAULT

total = fail = err = skip = 0
rows = []
for path in glob.glob(os.path.join(pattern, '*.xml')):
    root = ET.parse(path).getroot()
    n = int(root.get('tests', 0))
    total += n
    fail += int(root.get('failures', 0))
    err += int(root.get('errors', 0))
    skip += int(root.get('skipped', 0))
    rows.append((root.get('name'), n, root.get('failures'), root.get('errors')))

for row in sorted(rows):
    print(row)
print('TOTAL tests', total, 'failures', fail, 'errors', err, 'skipped', skip)
