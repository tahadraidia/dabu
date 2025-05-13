from sys import argv
from dabu import dump

if __name__ == "__main__":
	if (len(argv) - 1) < 1:
		print("{} <blob path>".format(argv[0]));
		exit(0);
	blob = argv[1]
	assemblies = dump(blob, False)
	print(assemblies)
	print(len(assemblies))


