import re
from sys import argv
from dabu import dump

regex = r'^(System\.|Mono\.|.*_Microsoft|Microsoft|Xamarin|mscorlib|Newtonsoft|Java.Interop)'

if __name__ == "__main__":
	if (len(argv) - 1) < 1:
		print("{} <blob path>".format(argv[0]));
		exit(0);
	blob = argv[1]
	pattern = re.compile(regex)
	assemblies = dump(blob, False)
	if (len(assemblies) > 0):
		dlls = list(filter(lambda i: not pattern.match(i['name']), assemblies))
		print(dlls)


