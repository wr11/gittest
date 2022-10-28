
# coding: utf-8

import re
# import random


OPCODE_H = 'Include/opcode.h'
OPCODE_PY = 'Lib/opcode.py'
OPCODE_RE = re.compile(
	r'^#define\s+(?P<name>[A-Z0-9_]+)\s+(?P<code>\d+)(?P<extra>.*)'
)
OPCODE_PY_RE = re.compile(
	r"(?P<name>[A-Z0-9+_]+)[, =']+(?P<code>\d+)"
)
HAVE_ARGUMENT_COUNT = 105  # 随手编的, 差不多就好


def is_opcode(line):
	return OPCODE_RE.match(line)

def set_opcode(opcodes, op, code):
	code_to_op = {j: i for i, j in opcodes.iteritems()}
	if code not in code_to_op:
		opcodes[op] = code
	else:
		old = opcodes[op]
		opcodes[op] = code
		opcodes[code_to_op[code]] = old

def make_contiguous(opcodes, ops):
	fst = min([opcodes[i] for i in ops])
	for idx, op in enumerate(ops):
		set_opcode(opcodes, op, fst + idx)

def fix_opcodes_offsets(opcodes):
	make_contiguous(opcodes, ['SLICE', 'SLICE_1', 'SLICE_2', 'SLICE_3'])
	make_contiguous(opcodes, ['STORE_SLICE', 'STORE_SLICE_1', 'STORE_SLICE_2', 'STORE_SLICE_3'])
	make_contiguous(opcodes, ['DELETE_SLICE', 'DELETE_SLICE_1', 'DELETE_SLICE_2', 'DELETE_SLICE_3'])
	make_contiguous(opcodes, ['CALL_FUNCTION', 'CALL_FUNCTION_VAR', 'CALL_FUNCTION_KW', 'CALL_FUNCTION_VAR_KW'])
	return opcodes

def opcode(line):
	match = is_opcode(line)
	if match:
		return (match.group('name'),
				int(match.group('code')),
				match.group('extra'))

def scramble_subset(opcodes, rg):
	names = [name for name, code, extra in opcodes]
	# random.shuffle(rg)
	# opcodes = rg[:len(names)]
	opcodes = [code for name, code, extra in opcodes]
	return dict(zip(names, opcodes))

def scramble_opcodes(path):
	lines = []
	dont_have_arg = []
	have_arg = []

	with open(path, 'r') as file:
		file_lines = file.readlines()
		opcodes = filter(None, map(opcode, file_lines))

		have = False
		for name, code, extra in opcodes:
			if name == 'HAVE_ARGUMENT':
				have = True
				continue
			opcodes_set = have_arg if have else dont_have_arg
			opcodes_set.append((name, code, extra))

		dont_have_arg = scramble_subset(dont_have_arg, range(HAVE_ARGUMENT_COUNT))
		have_arg = scramble_subset(have_arg, range(HAVE_ARGUMENT_COUNT, 250))
		have_arg['HAVE_ARGUMENT'] = HAVE_ARGUMENT_COUNT
		opcodes = dict(dont_have_arg)
		opcodes.update(have_arg)
		opcodes = fix_opcodes_offsets(opcodes)

		for line in file_lines:
			match = is_opcode(line)
			if match:
				name = match.group('name')
				line = '#define {0} {1}{2}\n'.format(name, opcodes[name],
													 match.group('extra'))
			elif 'Also uses' in line:
				last_op = opcodes[name]
				line = '/* Also uses %s-%s */\n' % (last_op + 1, last_op + 3)
			lines.append(line)

	with open(path, 'w') as file:
		file.write(''.join(lines))


def update_opcode_py(h_path, py_path):
	with open(h_path, 'r') as f:
		file_lines = f.readlines()
		opcodes = filter(None, map(opcode, file_lines))
		opcodes = {i[0]: i[1] for i in opcodes}

	for i in ('SLICE', 'STORE_SLICE', 'DELETE_SLICE'):
		for j in xrange(4):
			old_name = '%s%s' % (i, ('_%s' % j) if j else '')
			new_name = '%s+%s' % (i, j)
			opcodes[new_name] = opcodes[old_name]

	with open(py_path, 'r') as f:
		file_lines = f.readlines()
		new_lines = []
		old_code = None
		for line in file_lines:
			m = OPCODE_PY_RE.search(line)
			if m and m.group('name') in opcodes:
				op = m.group('name')
				old_code = m.group('code')
				new_code = str(opcodes[op])
				line = line.replace(old_code, new_code)
			elif old_code and old_code in line:
				line = line.replace(old_code, new_code)
			new_lines.append(line)

	with open(py_path, 'w') as f:
		f.write(''.join(new_lines))



if __name__ == '__main__':
	scramble_opcodes(OPCODE_H)
	update_opcode_py(OPCODE_H, OPCODE_PY)
