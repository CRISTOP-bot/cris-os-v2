#include "lcp.h"
#include "console.h"
#include "fs.h"
#include "kstring.h"
#include <stddef.h>
#include <stdbool.h>

#define MAX_PACKAGES 32
#define MAX_DEPENDENCIES 8
#define MAX_FILES 8
#define REPO_BUFFER_SIZE 8192

struct lcp_package {
	const char *name;
	const char *version;
	const char *description;
	const char *arch;
	const char *maintainer;
	const char *license;
	const char *repo;
	const char *dependencies[MAX_DEPENDENCIES];
	size_t dependency_count;
	const char *files[MAX_FILES];
	size_t file_count;
	unsigned int size;
	bool installed;
};

typedef struct lcp_package lcp_package_t;

static lcp_package_t packages[MAX_PACKAGES];
static size_t package_count;
static char repo_buffer[REPO_BUFFER_SIZE];
static bool repo_loaded;

static void lcp_print(const char *s)
{
	console_print(s);
}

static const char *lcp_trim(const char *s)
{
	while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n')
		++s;
	return s;
}

static void lcp_rtrim(char *s)
{
	size_t len = 0;
	while (s[len])
		++len;
	while (len > 0 && (s[len - 1] == ' ' || s[len - 1] == '\t' ||
	       s[len - 1] == '\r' || s[len - 1] == '\n')) {
		s[len - 1] = '\0';
		--len;
	}
}

static void lcp_split_list(const char *text, const char **out,
			   size_t *count, size_t max_count)
{
	*count = 0;
	const char *p = text;
	while (p && *p && *count < max_count) {
		while (*p == ' ' || *p == ',')
			++p;
		if (!*p)
			break;
		out[*count] = p;
		while (*p && *p != ',')
			++p;
		if (*p == ',') {
			*((char *)p) = '\0';
			++p;
		}
		*count += 1;
	}
}

static void lcp_parse_package_block(char *block)
{
	if (package_count >= MAX_PACKAGES)
		return;
	lcp_package_t *pkg = &packages[package_count];
	pkg->name = 0;
	pkg->version = 0;
	pkg->description = 0;
	pkg->arch = 0;
	pkg->maintainer = 0;
	pkg->license = 0;
	pkg->repo = 0;
	pkg->dependency_count = 0;
	pkg->file_count = 0;
	pkg->size = 0;
	pkg->installed = false;

	char *line = block;
	while (line && *line) {
		char *eol = line;
		while (*eol && *eol != '\n')
			++eol;
		if (*eol == '\n') {
			*eol = '\0';
			++eol;
		}
		lcp_rtrim(line);
		const char *value = 0;
		if (kstrncmp(line, "name:", 5) == 0) {
			value = lcp_trim(line + 5);
			pkg->name = value;
		} else if (kstrncmp(line, "version:", 8) == 0) {
			value = lcp_trim(line + 8);
			pkg->version = value;
		} else if (kstrncmp(line, "description:", 12) == 0) {
			value = lcp_trim(line + 12);
			pkg->description = value;
		} else if (kstrncmp(line, "arch:", 5) == 0) {
			value = lcp_trim(line + 5);
			pkg->arch = value;
		} else if (kstrncmp(line, "maintainer:", 11) == 0) {
			value = lcp_trim(line + 11);
			pkg->maintainer = value;
		} else if (kstrncmp(line, "license:", 8) == 0) {
			value = lcp_trim(line + 8);
			pkg->license = value;
		} else if (kstrncmp(line, "dependencies:", 13) == 0) {
			value = lcp_trim(line + 13);
			lcp_split_list(value, pkg->dependencies,
				       &pkg->dependency_count, MAX_DEPENDENCIES);
		} else if (kstrncmp(line, "files:", 6) == 0) {
			value = lcp_trim(line + 6);
			lcp_split_list(value, pkg->files,
				       &pkg->file_count, MAX_FILES);
		} else if (kstrncmp(line, "size:", 5) == 0) {
			value = lcp_trim(line + 5);
			unsigned int size = 0;
			while (*value >= '0' && *value <= '9') {
				size = size * 10 + (unsigned int)(*value - '0');
				++value;
			}
			pkg->size = size;
		} else if (kstrncmp(line, "repo:", 5) == 0) {
			value = lcp_trim(line + 5);
			pkg->repo = value;
		}
		line = eol;
	}

	if (pkg->name && pkg->version)
		package_count += 1;
}

static bool lcp_parse_repo(const char *data, size_t size)
{
	if (size >= REPO_BUFFER_SIZE)
		return false;
	for (size_t i = 0; i < size; ++i)
		repo_buffer[i] = (data[i] == '\r') ? '\n' : data[i];
	repo_buffer[size] = '\0';

	package_count = 0;
	char *block = repo_buffer;
	char *p = repo_buffer;
	while (*p) {
		if (p[0] == '-' && p[1] == '-' && p[2] == '-' &&
		    (p[3] == '\n' || p[3] == '\0')) {
			*p = '\0';
			lcp_parse_package_block(block);
			p += 3;
			if (*p == '\n')
				++p;
			block = p;
		} else {
			++p;
		}
	}
	if (block && *block)
		lcp_parse_package_block(block);
	return package_count > 0;
}

static const char default_repo_data[] =
	"name:nano-nucleos\n"
	"version:1.0.0\n"
	"description:Editor de texto simple para NucleOS\n"
	"arch:x86_64\n"
	"maintainer:NucleOS Team\n"
	"license:MIT\n"
	"dependencies:libc,termcap\n"
	"files:bin/nano-nucleos,share/nano-nucleos/help.txt\n"
	"size:32000\n"
	"repo:main\n"
	"---\n"
	"name:editor-lite\n"
	"version:0.5.0\n"
	"description:Editor de texto ligero para NucleOS\n"
	"arch:x86_64\n"
	"maintainer:NucleOS Team\n"
	"license:MIT\n"
	"dependencies:\n"
	"files:bin/editor-lite\n"
	"size:12500\n"
	"repo:main\n"
	"---\n"
	"name:textpad\n"
	"version:0.9.1\n"
	"description:Editor de texto basado en terminal para NucleOS\n"
	"arch:x86_64\n"
	"maintainer:NucleOS Team\n"
	"license:Apache-2.0\n"
	"dependencies:libc\n"
	"files:bin/textpad\n"
	"size:21200\n"
	"repo:main\n"
	"---\n"
	"name:libc\n"
	"version:1.0.0\n"
	"description:Biblioteca estandar minima para NucleOS\n"
	"arch:x86_64\n"
	"maintainer:NucleOS Team\n"
	"license:MIT\n"
	"dependencies:\n"
	"files:lib/libc.so\n"
	"size:45000\n"
	"repo:main\n"
	"---\n"
	"name:termcap\n"
	"version:1.2.3\n"
	"description:Biblioteca de compatibilidad con terminal para NucleOS\n"
	"arch:x86_64\n"
	"maintainer:NucleOS Team\n"
	"license:BSD-2-Clause\n"
	"dependencies:\n"
	"files:lib/termcap.so\n"
	"size:18000\n"
	"repo:main\n"
	"---\n"
	"name:snake-game\n"
	"version:1.0.0\n"
	"description:Juego clasico de Snake para NucleOS\n"
	"arch:x86_64\n"
	"maintainer:NucleOS Team\n"
	"license:MIT\n"
	"dependencies:\n"
	"files:games/snake-game\n"
	"size:8200\n"
	"repo:games\n"
	"---\n"
	"name:tetris-game\n"
	"version:1.0.0\n"
	"description:Juego clasico de Tetris para NucleOS\n"
	"arch:x86_64\n"
	"maintainer:NucleOS Team\n"
	"license:MIT\n"
	"dependencies:\n"
	"files:games/tetris-game\n"
	"size:11500\n"
	"repo:games\n"
	"---\n"
	"name:pong-game\n"
	"version:1.0.0\n"
	"description:Juego clasico de Pong con CPU para NucleOS\n"
	"arch:x86_64\n"
	"maintainer:NucleOS Team\n"
	"license:MIT\n"
	"dependencies:\n"
	"files:games/pong-game\n"
	"size:7800\n"
	"repo:games\n"
	"---\n"
	"name:2048-game\n"
	"version:1.0.0\n"
	"description:Juego puzzle 2048 para NucleOS\n"
	"arch:x86_64\n"
	"maintainer:NucleOS Team\n"
	"license:MIT\n"
	"dependencies:\n"
	"files:games/2048-game\n"
	"size:9400\n"
	"repo:games\n"
	"---\n"
	"name:ttt-game\n"
	"version:1.0.0\n"
	"description:Tic-Tac-Toe con IA minimax para NucleOS\n"
	"arch:x86_64\n"
	"maintainer:NucleOS Team\n"
	"license:MIT\n"
	"dependencies:\n"
	"files:games/ttt-game\n"
	"size:6200\n"
	"repo:games\n"
	"---\n"
	"name:minesweeper\n"
	"version:1.0.0\n"
	"description:Juego Minesweeper clasico para NucleOS\n"
	"arch:x86_64\n"
	"maintainer:NucleOS Team\n"
	"license:MIT\n"
	"dependencies:\n"
	"files:games/minesweeper\n"
	"size:8900\n"
	"repo:games\n"
	"---\n"
	"name:breakout\n"
	"version:1.0.0\n"
	"description:Juego Breakout con bloques y pelota para NucleOS\n"
	"arch:x86_64\n"
	"maintainer:NucleOS Team\n"
	"license:MIT\n"
	"dependencies:\n"
	"files:games/breakout\n"
	"size:10200\n"
	"repo:games\n"
	"---\n"
	"name:memory-game\n"
	"version:1.0.0\n"
	"description:Juego de memoria con cartas para NucleOS\n"
	"arch:x86_64\n"
	"maintainer:NucleOS Team\n"
	"license:MIT\n"
	"dependencies:\n"
	"files:games/memory-game\n"
	"size:7100\n"
	"repo:games\n"
	"---\n"
	"name:calc-app\n"
	"version:2.0.0\n"
	"description:Calculadora de expresiones para NucleOS\n"
	"arch:x86_64\n"
	"maintainer:NucleOS Team\n"
	"license:MIT\n"
	"dependencies:\n"
	"files:bin/calc-app\n"
	"size:4500\n"
	"repo:tools\n"
	"---\n"
	"name:hexview\n"
	"version:1.0.0\n"
	"description:Visor hexadecimal de archivos para NucleOS\n"
	"arch:x86_64\n"
	"maintainer:NucleOS Team\n"
	"license:MIT\n"
	"dependencies:\n"
	"files:bin/hexview\n"
	"size:3200\n"
	"repo:tools\n"
	"---\n"
	"name:sysinfo\n"
	"version:1.0.0\n"
	"description:Herramienta de informacion del sistema para NucleOS\n"
	"arch:x86_64\n"
	"maintainer:NucleOS Team\n"
	"license:MIT\n"
	"dependencies:\n"
	"files:bin/sysinfo\n"
	"size:5100\n"
	"repo:tools\n"
	"---\n"
	"name:filemanager\n"
	"version:0.8.0\n"
	"description:Gestor de archivos basado en terminal para NucleOS\n"
	"arch:x86_64\n"
	"maintainer:NucleOS Team\n"
	"license:MIT\n"
	"dependencies:libc\n"
	"files:bin/filemanager\n"
	"size:8700\n"
	"repo:tools\n"
	"---\n"
	"name:htop-lite\n"
	"version:0.3.0\n"
	"description:Monitor de procesos y memoria para NucleOS\n"
	"arch:x86_64\n"
	"maintainer:NucleOS Team\n"
	"license:GPL-2.0\n"
	"dependencies:\n"
	"files:bin/htop-lite\n"
	"size:6300\n"
	"repo:tools\n"
	"---\n"
	"name:gpu-driver\n"
	"version:0.1.0\n"
	"description:Driver basico de VGA para NucleOS\n"
	"arch:x86_64\n"
	"maintainer:NucleOS Team\n"
	"license:MIT\n"
	"dependencies:\n"
	"files:drivers/vga-driver.so\n"
	"size:24000\n"
	"repo:drivers\n"
	"---\n"
	"name:net-driver\n"
	"version:0.1.0\n"
	"description:Driver de red RTL8139 para NucleOS\n"
	"arch:x86_64\n"
	"maintainer:NucleOS Team\n"
	"license:MIT\n"
	"dependencies:\n"
	"files:drivers/rtl8139.so\n"
	"size:31000\n"
	"repo:drivers\n"
	"---\n"
	"name:input-driver\n"
	"version:1.0.0\n"
	"description:Driver de entrada PS/2 para NucleOS\n"
	"arch:x86_64\n"
	"maintainer:NucleOS Team\n"
	"license:MIT\n"
	"dependencies:\n"
	"files:drivers/ps2-input.so\n"
	"size:9500\n"
	"repo:drivers\n"
	"---\n"
	"name:libc-dev\n"
	"version:1.0.0\n"
	"description:Archivos de desarrollo de libc para NucleOS\n"
	"arch:x86_64\n"
	"maintainer:NucleOS Team\n"
	"license:MIT\n"
	"dependencies:libc\n"
	"files:include/stdio.h,include/stdlib.h,include/string.h\n"
	"size:12000\n"
	"repo:dev\n"
	"---\n"
	"name:gcc-nucleos\n"
	"version:4.9.0\n"
	"description:Compilador GCC para NucleOS\n"
	"arch:x86_64\n"
	"maintainer:NucleOS Team\n"
	"license:GPL-3.0\n"
	"dependencies:libc\n"
	"files:bin/gcc,libexec/gcc/nucleos/4.9/cc1\n"
	"size:256000\n"
	"repo:dev\n"
	"---\n"
	"name:make-nucleos\n"
	"version:4.2.1\n"
	"description:GNU Make para NucleOS\n"
	"arch:x86_64\n"
	"maintainer:NucleOS Team\n"
	"license:GPL-3.0\n"
	"dependencies:\n"
	"files:bin/make\n"
	"size:42000\n"
	"repo:dev\n"
	"---\n"
	"name:binutils-nucleos\n"
	"version:2.30\n"
	"description:GNU Binutils (as, ld, objdump) para NucleOS\n"
	"arch:x86_64\n"
	"maintainer:NucleOS Team\n"
	"license:GPL-3.0\n"
	"dependencies:\n"
	"files:bin/as,bin/ld,bin/objdump\n"
	"size:89000\n"
	"repo:dev\n"
	"---\n"
	"name:bash-nucleos\n"
	"version:5.1.0\n"
	"description:Bash shell para NucleOS\n"
	"arch:x86_64\n"
	"maintainer:NucleOS Team\n"
	"license:GPL-3.0\n"
	"dependencies:libc,termcap\n"
	"files:bin/bash\n"
	"size:120000\n"
	"repo:extra\n"
	"---\n"
	"name:python-nucleos\n"
	"version:3.9.0\n"
	"description:Python 3 interprete para NucleOS\n"
	"arch:x86_64\n"
	"maintainer:NucleOS Team\n"
	"license:PSF\n"
	"dependencies:libc\n"
	"files:bin/python3,lib/python3.9/\n"
	"size:512000\n"
	"repo:extra\n"
	"---\n"
	"name:wget-nucleos\n"
	"version:1.21.0\n"
	"description:Descargador de archivos HTTP para NucleOS\n"
	"arch:x86_64\n"
	"maintainer:NucleOS Team\n"
	"license:GPL-3.0\n"
	"dependencies:libc\n"
	"files:bin/wget\n"
	"size:28000\n"
	"repo:extra\n"
	"---\n"
	"name:curl-nucleos\n"
	"version:7.80.0\n"
	"description:Cliente URL multiprotocolo para NucleOS\n"
	"arch:x86_64\n"
	"maintainer:NucleOS Team\n"
	"license:MIT\n"
	"dependencies:libc\n"
	"files:bin/curl\n"
	"size:35000\n"
	"repo:extra\n";

bool lcp_init(void)
{
	const struct fs_file *file = fs_find("lcp_repo.txt");
	if (file) {
		if (lcp_parse_repo((const char *)file->data, file->size)) {
			repo_loaded = true;
			return true;
		}
	}
	lcp_parse_repo(default_repo_data, sizeof(default_repo_data) - 1);
	repo_loaded = true;
	return true;
}

static lcp_package_t *lcp_find_package(const char *name)
{
	for (size_t i = 0; i < package_count; ++i) {
		if (kstrcmp(packages[i].name, name) == 0)
			return &packages[i];
	}
	return 0;
}

static bool lcp_contains(const char *text, const char *term)
{
	while (*text) {
		const char *a = text;
		const char *b = term;
		while (*a && *b) {
			char ca = *a;
			char cb = *b;
			if (ca >= 'A' && ca <= 'Z')
				ca += 'a' - 'A';
			if (cb >= 'A' && cb <= 'Z')
				cb += 'a' - 'A';
			if (ca != cb)
				break;
			++a;
			++b;
		}
		if (!*b)
			return true;
		++text;
	}
	return false;
}

static void lcp_print_line(const char *label, const char *value)
{
	char buffer[256];
	buffer[0] = '\0';
	kstrcpy(buffer, label, sizeof(buffer));
	if (value)
		kstrcat(buffer, value, sizeof(buffer));
	console_print(buffer);
	console_print("\n");
}

static void lcp_format_number(unsigned int value, char *out, size_t max_len)
{
	if (max_len == 0)
		return;
	if (value == 0) {
		if (max_len >= 2) {
			out[0] = '0';
			out[1] = '\0';
		} else {
			out[0] = '\0';
		}
		return;
	}
	char rev[16];
	int rp = 0;
	while (value > 0 && rp < (int)sizeof(rev)) {
		rev[rp++] = '0' + (value % 10);
		value /= 10;
	}
	int pos = 0;
	while (rp > 0 && pos + 1 < (int)max_len)
		out[pos++] = rev[--rp];
	out[pos] = '\0';
}

static void lcp_print_package_info(const lcp_package_t *pkg)
{
	lcp_print_line("name: ", pkg->name ? pkg->name : "");
	lcp_print_line("version: ", pkg->version ? pkg->version : "");
	lcp_print_line("description: ", pkg->description ? pkg->description : "");
	char number[32];
	lcp_format_number(pkg->size, number, sizeof(number));
	char size_line[64];
	kstrcpy(size_line, "size: ", sizeof(size_line));
	kstrcat(size_line, number, sizeof(size_line));
	kstrcat(size_line, " bytes", sizeof(size_line));
	console_print(size_line);
	console_print("\n");
	if (pkg->dependency_count == 0) {
		lcp_print_line("dependencies: ", "none");
	} else {
		char output[256];
		output[0] = '\0';
		for (size_t i = 0; i < pkg->dependency_count; ++i) {
			if (i > 0)
				kstrcat(output, ", ", sizeof(output));
			kstrcat(output, pkg->dependencies[i], sizeof(output));
		}
		lcp_print_line("dependencies: ", output);
	}
	lcp_print_line("repository: ", pkg->repo ? pkg->repo : "");
	lcp_print_line("author: ", pkg->maintainer ? pkg->maintainer : "");
	lcp_print_line("license: ", pkg->license ? pkg->license : "");
}

static void lcp_print_help(void)
{
	console_print("lcp commands:\n");
	console_print("  lcp help [command]\n");
	console_print("  lcp search <term>\n");
	console_print("  lcp info <package>\n");
	console_print("  lcp install <package>\n");
	console_print("  lcp remove <package>\n");
	console_print("  lcp update\n");
	console_print("  lcp upgrade [package]\n");
	console_print("  lcp list [--installed|--available|--upgradable]\n");
	console_print("  lcp files <package>\n");
	console_print("  lcp depends <package>\n");
	console_print("  lcp verify <package>\n");
}

static void lcp_list_available(void)
{
	for (size_t i = 0; i < package_count; ++i) {
		console_print(packages[i].name);
		console_print("\n");
	}
}

static void lcp_list_installed(void)
{
	for (size_t i = 0; i < package_count; ++i) {
		if (packages[i].installed) {
			console_print(packages[i].name);
			console_print("\n");
		}
	}
}

static bool lcp_has_dependents(const char *name)
{
	for (size_t i = 0; i < package_count; ++i) {
		if (!packages[i].installed)
			continue;
		for (size_t j = 0; j < packages[i].dependency_count; ++j) {
			if (kstrcmp(packages[i].dependencies[j], name) == 0)
				return true;
		}
	}
	return false;
}

static void lcp_install_package(lcp_package_t *pkg, bool no_deps)
{
	if (pkg->installed) {
		console_print("Package already installed.\n");
		return;
	}
	if (!no_deps) {
		for (size_t i = 0; i < pkg->dependency_count; ++i) {
			lcp_package_t *dep = lcp_find_package(pkg->dependencies[i]);
			if (dep && !dep->installed)
				lcp_install_package(dep, false);
		}
	}
	pkg->installed = true;
	console_print("Installed ");
	console_print(pkg->name);
	console_print("\n");
}

static void lcp_remove_package(lcp_package_t *pkg, bool purge)
{
	if (!pkg->installed) {
		console_print("Package is not installed.\n");
		return;
	}
	if (lcp_has_dependents(pkg->name)) {
		console_print("Cannot remove package because another installed package depends on it.\n");
		return;
	}
	pkg->installed = false;
	console_print("Removed ");
	console_print(pkg->name);
	if (purge)
		console_print(" and purged configuration.");
	console_print("\n");
}

static void lcp_upgrade_package(lcp_package_t *pkg)
{
	if (!pkg->installed) {
		console_print("Package is not installed.\n");
		return;
	}
	console_print("Upgraded ");
	console_print(pkg->name);
	console_print("\n");
}

static void lcp_show_files(const lcp_package_t *pkg)
{
	if (!pkg) {
		console_print("Package not found.\n");
		return;
	}
	for (size_t i = 0; i < pkg->file_count; ++i) {
		console_print(pkg->files[i]);
		console_print("\n");
	}
}

static void lcp_show_dependencies(const lcp_package_t *pkg)
{
	if (!pkg) {
		console_print("Package not found.\n");
		return;
	}
	if (pkg->dependency_count == 0) {
		console_print("No dependencies.\n");
		return;
	}
	for (size_t i = 0; i < pkg->dependency_count; ++i) {
		console_print(pkg->dependencies[i]);
		console_print("\n");
	}
}

static void lcp_verify_package(const lcp_package_t *pkg)
{
	if (!pkg) {
		console_print("Package not found.\n");
		return;
	}
	if (!pkg->installed) {
		console_print("Package is not installed.\n");
		return;
	}
	console_print("Package is installed and appears healthy.\n");
}

static void lcp_search(const char *term)
{
	bool found = false;
	for (size_t i = 0; i < package_count; ++i) {
		if (lcp_contains(packages[i].name, term) ||
		    lcp_contains(packages[i].description ? packages[i].description : "", term)) {
			console_print(packages[i].name);
			console_print("\n");
			found = true;
		}
	}
	if (!found)
		console_print("No matching packages.\n");
}

static const char *lcp_next_token(const char *s, char *token, size_t max_len)
{
	while (*s == ' ')
		++s;
	size_t pos = 0;
	while (*s && *s != ' ' && pos + 1 < max_len)
		token[pos++] = *s++;
	token[pos] = '\0';
	while (*s == ' ')
		++s;
	return s;
}

int lcp_handle_command(const char *args)
{
	char token[64];
	const char *rest = lcp_next_token(args, token, sizeof(token));
	if (token[0] == '\0' || kstrcmp(token, "help") == 0) {
		lcp_print_help();
		return 0;
	}
	if (kstrcmp(token, "search") == 0) {
		if (*rest == '\0') {
			lcp_print("search requires a term.\n");
			return 0;
		}
		lcp_search(rest);
		return 0;
	}
	if (kstrcmp(token, "info") == 0) {
		if (*rest == '\0') {
			lcp_print("info requires a package name.\n");
			return 0;
		}
		lcp_package_t *pkg = lcp_find_package(rest);
		if (!pkg) {
			lcp_print("Package not found.\n");
			return 0;
		}
		lcp_print_package_info(pkg);
		return 0;
	}
	if (kstrcmp(token, "list") == 0) {
		if (kstrcmp(rest, "--available") == 0) {
			lcp_list_available();
			return 0;
		}
		if (kstrcmp(rest, "--installed") == 0) {
			lcp_list_installed();
			return 0;
		}
		lcp_list_installed();
		return 0;
	}
	if (kstrcmp(token, "install") == 0) {
		bool no_deps = false;
		const char *name = rest;
		if (kstrncmp(rest, "--no-deps", 9) == 0) {
			no_deps = true;
			name = lcp_trim(rest + 9);
		}
		if (*name == '\0') {
			lcp_print("install requires a package name.\n");
			return 0;
		}
		lcp_package_t *pkg = lcp_find_package(name);
		if (!pkg) {
			lcp_print("Package not found.\n");
			return 0;
		}
		if (pkg->installed) {
			lcp_print("Package already installed.\n");
			return 0;
		}
		lcp_install_package(pkg, no_deps);
		return 0;
	}
	if (kstrcmp(token, "remove") == 0) {
		bool purge = false;
		const char *name = rest;
		if (kstrncmp(rest, "--purge", 7) == 0) {
			purge = true;
			name = lcp_trim(rest + 7);
		}
		if (*name == '\0') {
			lcp_print("remove requires a package name.\n");
			return 0;
		}
		lcp_package_t *pkg = lcp_find_package(name);
		if (!pkg) {
			lcp_print("Package not found.\n");
			return 0;
		}
		lcp_remove_package(pkg, purge);
		return 0;
	}
	if (kstrcmp(token, "update") == 0) {
		if (lcp_init())
			lcp_print("Repository metadata updated.\n");
		else
			lcp_print("Failed to update repository metadata.\n");
		return 0;
	}
	if (kstrcmp(token, "upgrade") == 0) {
		if (*rest == '\0') {
			for (size_t i = 0; i < package_count; ++i) {
				if (packages[i].installed)
					lcp_upgrade_package(&packages[i]);
			}
			return 0;
		}
		lcp_package_t *pkg = lcp_find_package(rest);
		if (!pkg) {
			lcp_print("Package not found.\n");
			return 0;
		}
		lcp_upgrade_package(pkg);
		return 0;
	}
	if (kstrcmp(token, "files") == 0) {
		if (*rest == '\0') {
			lcp_print("files requires a package name.\n");
			return 0;
		}
		lcp_package_t *pkg = lcp_find_package(rest);
		if (!pkg) {
			lcp_print("Package not found.\n");
			return 0;
		}
		lcp_show_files(pkg);
		return 0;
	}
	if (kstrcmp(token, "depends") == 0) {
		if (*rest == '\0') {
			lcp_print("depends requires a package name.\n");
			return 0;
		}
		lcp_package_t *pkg = lcp_find_package(rest);
		if (!pkg) {
			lcp_print("Package not found.\n");
			return 0;
		}
		lcp_show_dependencies(pkg);
		return 0;
	}
	if (kstrcmp(token, "verify") == 0) {
		if (*rest == '\0') {
			lcp_print("verify requires a package name.\n");
			return 0;
		}
		lcp_package_t *pkg = lcp_find_package(rest);
		if (!pkg) {
			lcp_print("Package not found.\n");
			return 0;
		}
		lcp_verify_package(pkg);
		return 0;
	}
	lcp_print("Unknown lcp command. Type 'lcp help' for commands.\n");
	return 0;
}
