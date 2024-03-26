'''
在本地启动指定数量的节点。
usage: local.py [-h] -ns NS -nw NW -exec EXEC [-lr] [-lr_normal] [-multi_customer] [-verbose VERBOSE]

将为 exec 传递至少三个参数：argv[1]=config_filename, argv[2]=log_filename, argv[3]=role
'''
import argparse
import json
import sys, os
import subprocess

# 清除旧 log 文件
log_directory = os.path.join(os.getcwd(), 'log')
if os.path.exists(log_directory) and os.path.isdir(log_directory):
	for file in os.listdir(log_directory):
		if file.startswith('log'):
			file_path = os.path.join(log_directory, file)
			os.remove(file_path)

# 通过 ArgumentParser 读取 -k=v/-k v 格式的命令行参数
parser = argparse.ArgumentParser()

parser.add_argument('-ns', required=True, help='number of servers')
parser.add_argument('-nw', required=True, help='number of workers')
parser.add_argument('-exec', required=True, help='the program to be run')

parser.add_argument('-lr', action='store_true', help='whether to run lr_ps test')
parser.add_argument('-lr_normal', action='store_true', help='whether to run lr_normal test')
parser.add_argument('-multi_customer', action='store_true', help='whether to run all workers in one process. default: false')
parser.add_argument('-verbose', help='log level. default: 1')

# 获取配置
args = parser.parse_args()

num_servers = int(args.ns) # 注意结果是 str
num_workers = int(args.nw)
program = args.exec

scheduler_uri = '127.0.0.1'
scheduler_port = 8000

verbose = 1
multi_customer = False
use_lr = False
use_lr_normal = False

if args.verbose:
	verbose = int(args.verbose)
if args.multi_customer:
	multi_customer = args.multi_customer
if args.lr:
	use_lr = args.lr
if args.lr_normal:
	use_lr = True
	use_lr_normal = args.lr_normal
	num_servers = 0 # 只运行一个进程即可
	num_workers = 0

processes = []

def create_cfg(role, cfg_name):
	cfg = {}
	cfg['PS_NUM_SERVER'] = num_servers
	cfg['PS_NUM_WORKER'] = num_workers
	cfg['PS_ROLE'] = role
	cfg['PS_SCHEDULER_URI'] = scheduler_uri
	cfg['PS_SCHEDULER_PORT'] = scheduler_port
	cfg['PS_VERBOSE'] = verbose
	# if role == 'scheduler':
	# 	cfg['PS_VERBOSE'] = 1

	if use_lr:
		cfg['DATA_DIR'] = './LR'
		cfg['NUM_FEATURE'] = 123
		cfg['SYNC_MODE'] = 0 # 0: sync, 1: async
		cfg['TEST_PERIOD'] = 1 # 100
		cfg['C'] = 1 # 1

		cfg['BATCH_SIZE'] = 500 # 500
		cfg['ITERATION'] = 250 # 1000
		cfg['LEARNING_RATE'] = 0.01
		# cfg['USE_OLD_MODEL'] = 'lr_ps_normal_100'
		cfg['USE_ADAM'] = 1

	with open(cfg_name, 'w') as f:
		json.dump(cfg, f, indent=4)

def start_node(role, cfg_name, log_name):
	create_cfg(role, cfg_name)
	# start_program
	try:
		process = subprocess.Popen([program] + [cfg_name, log_name, role]) # + args
		processes.append(process)
	except FileNotFoundError:
		print(f"Error: Program '{program}' not found.")
	except Exception as e:
		print(f"An error occurred: {e}")

# start scheduler
start_node('scheduler', '.\log\config_H.json', '.\log\log_H.txt')

# start servers
for i in range(0, num_servers):
	start_node('server', '.\log\config_S.json', '.\log\log_S' + str(i) + '.txt')

for i in range(0, num_workers if not multi_customer else 1):
	start_node('worker', '.\log\config_W.json', '.\log\log_W' + str(i) + '.txt')

print('all nodes started. training.')

for process in processes:
	process.wait()

print('all processes done. exiting.')

