#ifndef CDROM_H
#define CDROM_H

void cdrom_init(const char *devpath);
void cdrom_eject(const char *devpath);
bool cdrom_media_present(const char *devpath);

#endif /* CDROM_H */

