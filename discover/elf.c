#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

#include <log/log.h>
#include "elf.h"

Elf *elf_open_image(const char *image)
{
	int fd;
	Elf *elf = NULL;
	int err;

	if (!image) {
		pb_log_fn("kernel image path is null\n");
		return NULL;
	}

	if ((elf_version(EV_CURRENT) == EV_NONE) ||
		((fd = open(image, O_RDONLY, 0)) == -1) ||
		(!(elf = elf_begin(fd, ELF_C_READ, NULL)))) {
		err = elf_errno();
		if (err)
			pb_log_fn("failed to read %s elf: %s\n",
				image, elf_errmsg(err));
	}

	return elf;
}

static bool elf_getnote_offset(Elf_Data * const edata,
			const char *namespace,
			const uint32_t type, GElf_Nhdr *nhdr,
			size_t *n_off, size_t *d_off)
{
	size_t off = 0;
	size_t next;

	/* Iterate through notes */
	while ((next = gelf_getnote(edata, off, nhdr, n_off, d_off)) > 0) {
		char *note_ns = (char *) edata->d_buf + (*n_off);
		if ((strcmp(note_ns, namespace) == 0) && (nhdr->n_type == type))
			return true;

		off = next;
	}
	return false;
}

void *elf_getnote_desc(Elf *elf,
		const char *namespace,
		uint32_t type)
{
	Elf_Scn *scn = NULL;
	Elf_Data *edata = NULL;
	GElf_Shdr shdr;
	GElf_Nhdr nhdr;

	size_t n_off;
	size_t d_off;
	void *desc = NULL;

	if (!elf || !namespace)
		return NULL;

	/* Iterate through sections */
	while ((scn = elf_nextscn(elf, scn))) {
		gelf_getshdr(scn, &shdr);

		/* ELF might have more than one SHT_NOTE section but
		   only one has the 'namespace' note */
		if (shdr.sh_type == SHT_NOTE) {
			edata = elf_getdata(scn, NULL);
			if (elf_getnote_offset(edata, namespace, type,
						&nhdr, &n_off, &d_off)) {
				desc = calloc(nhdr.n_descsz, sizeof(char));
				memcpy(desc, edata->d_buf + d_off,
					nhdr.n_descsz);
				break;
			}
		}
	}

	return desc;
}

