/* SPDX-License-Identifier: LGPL-2.1-only */
/*
 * Author: Weijie Gao <hackpascal@gmail.com>
 *
 * Linux serial port device operations
 */

#define _DEFAULT_SOURCE
#define _FILE_OFFSET_BITS	64
#include <errno.h>
#include <limits.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/file.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#define termios asmtermios
#include <asm/termbits.h>
#undef  termios
#include <termios.h>
#include <string.h>
#include <stdbool.h>
#include <ufprog/log.h>
#include <ufprog/osdef.h>
#include <ufprog/serial.h>

#ifndef TCGETS2
#define termios2	termios
#define TCGETS2		TCGETS
#define TCSETS2		TCSETS
#define c_ispeed	__c_ispeed
#define c_ospeed	__c_ospeed
#endif

struct os_serial_port {
	int fd;
	struct termios tty_old;
	uint32_t timeout_ms;
};

static ufprog_status fd_set_blocking(int fd, bool blocking)
{
	int ret, err;

	ret = fcntl(fd, F_GETFL);
	if (ret == -1) {
		err = errno;
		log_err("fcntl() failed with %d: %s\n", err, strerror(err));
		return UFP_FAIL;
	}

	if (blocking)
		ret &= ~O_NONBLOCK;
	else
		ret |= O_NONBLOCK;

	ret = fcntl(fd, F_SETFL, ret);
	if (ret != 0) {
		err = errno;
		log_err("fcntl() failed with %d: %s\n", err, strerror(err));
		return UFP_FAIL;
	}

	return UFP_OK;
}

ufprog_status UFPROG_API serial_port_open(const char *path, serial_port *outdev)
{
	ufprog_status ret;
	serial_port dev;
	int fd, rc, err;

	fd = open(path, O_RDWR | O_NOCTTY | O_NDELAY);
	if (fd < 0) {
		err = errno;
		log_err("open() failed for %s. error %d: %s\n", path, err, strerror(err));
		return UFP_FAIL;
	}

	if (!isatty(fd)) {
		log_err("%s is not a tty device\n", path);
		ret = UFP_FAIL;
		goto cleanup;
	}

	rc = flock(fd, LOCK_EX | LOCK_NB);
	if (rc == -1 && errno == EWOULDBLOCK) {
		log_err("%s is locked by another device\n", path);
		ret = UFP_FAIL;
		goto cleanup;
	}

	ret = fd_set_blocking(fd, true);
	if (ret) {
		log_err("Failed to set blocking fd for serial port\n");
		goto cleanup;
	}

	dev = calloc(1, sizeof(*dev));
	if (!dev) {
		log_err("No memory for serial port device\n");
		ret = UFP_NOMEM;
		goto cleanup;
	}

	rc = tcgetattr(dev->fd, &dev->tty_old);
	if (rc) {
		err = errno;
		log_err("tcgetattr() failed with %d: %s\n", err, strerror(err));
		ret = UFP_FAIL;
		goto cleanup_dev;
	}

	dev->fd = fd;

	*outdev = dev;

	return UFP_OK;

cleanup_dev:
	free(dev);

cleanup:
	close(fd);

	return ret;
}

ufprog_status UFPROG_API serial_port_close(serial_port dev)
{
	if (!dev)
		return UFP_INVALID_PARAMETER;

	tcsetattr(dev->fd, TCSANOW, &dev->tty_old);

	flock(dev->fd, LOCK_UN);

	close(dev->fd);

	free(dev);

	return UFP_OK;
}

ufprog_status UFPROG_API serial_port_set_config(serial_port dev, const struct serial_port_config *config)
{
	struct termios2 tio;
	struct termios tty;
	ufprog_status ret;
	int rc, st, err;

	if (!dev || !config)
		return UFP_INVALID_PARAMETER;

	if (config->stop_bits >= __MAX_SERIAL_STOP_BITS || config->parity >= __MAX_SERIAL_PARITY ||
	    (config->data_bits < 5 || config->data_bits > 8))
		return UFP_INVALID_PARAMETER;

	rc = tcgetattr(dev->fd, &tty);
	if (rc) {
		err = errno;
		log_err("tcgetattr() failed with %d: %s\n", err, strerror(err));
		return UFP_FAIL;
	}

	tty.c_cflag &= ~CSIZE;

	switch (config->data_bits) {
	case 5:
		tty.c_cflag |= CS5;
		break;

	case 6:
		tty.c_cflag |= CS6;
		break;

	case 7:
		tty.c_cflag |= CS7;
		break;

	case 8:
		tty.c_cflag |= CS8;
		break;
	}

	switch (config->stop_bits) {
	case SERIAL_STOP_BITS_2:
		tty.c_cflag |= CSTOPB;
		break;

	default:
		tty.c_cflag &= ~CSTOPB;
	}

	tty.c_cflag &= ~(PARENB | PARODD);

	switch (config->parity) {
	case SERIAL_PARITY_NONE:
		break;

	case SERIAL_PARITY_ODD:
		tty.c_cflag |= PARODD;
		break;

	case SERIAL_PARITY_EVEN:
		break;

	case SERIAL_PARITY_MARK:
		tty.c_cflag |= PARODD | CMSPAR;
		break;

	case SERIAL_PARITY_SPACE:
		tty.c_cflag |= CMSPAR;
		break;
	}

	if (config->parity != SERIAL_PARITY_NONE)
		tty.c_cflag |= PARENB;

	/* Don't enable parity check here */
	tty.c_iflag &= ~(INPCK | PARMRK);

	/* Flow control */
	tty.c_cflag &= ~CRTSCTS;
	tty.c_iflag &= ~(IXON | IXOFF);

	if (config->fc == SERIAL_FC_RTS_CTS) {
		tty.c_cflag |= CRTSCTS;
	} else if (config->fc == SERIAL_FC_XON_XOFF) {
		tty.c_iflag |= IXON | IXOFF;
		tty.c_cc[VSTOP] = config->xoff;
		tty.c_cc[VSTART] = config->xon;
	}

	tty.c_cflag |= CLOCAL | CREAD;

	tty.c_lflag &= ~(ISIG | ICANON | ECHO | ECHOE | ECHOK | ECHONL | IEXTEN);
	tty.c_iflag &= ~(IGNBRK | INLCR | IGNCR | ICRNL | IXANY | IGNPAR);
	tty.c_oflag &= ~(OPOST | ONLCR | OCRNL | ONOCR | ONLRET | OFILL);

	rc = tcsetattr(dev->fd, TCSANOW, &tty);
	if (rc) {
		err = errno;
		log_err("tcsetattr() failed with %d: %s\n", err, strerror(err));
		return UFP_FAIL;
	}

	rc = ioctl(dev->fd, TIOCMGET, &st);
	if (rc < 0) {
		err = errno;
		log_err("ioctl() failed with %d: %s\n", err, strerror(err));
		return UFP_FAIL;
	}

	if (config->fc == SERIAL_FC_DTR_DSR)
		st |= TIOCM_DTR | TIOCM_DSR;
	else
		st &= ~(TIOCM_DTR | TIOCM_DSR);

	rc = ioctl(dev->fd, TIOCMSET, &st);
	if (rc < 0) {
		err = errno;
		log_err("ioctl() failed with %d: %s\n", err, strerror(err));
		return UFP_FAIL;
	}

	rc = ioctl(dev->fd, TCGETS2, &tio);
	if (rc < 0) {
		err = errno;
		log_err("ioctl() failed with %d: %s\n", err, strerror(err));
		return UFP_FAIL;
	}

	tio.c_cflag &= ~CBAUD;
	tio.c_cflag |= BOTHER;
	tio.c_ispeed = config->baudrate;
	tio.c_ospeed = config->baudrate;

	rc = ioctl(dev->fd, TCSETS2, &tio);
	if (rc < 0) {
		err = errno;
		log_err("ioctl() failed with %d: %s\n", err, strerror(err));
		return UFP_FAIL;
	}

	if (config->timeout_ms) {
		ret = fd_set_blocking(dev->fd, true);
		if (ret) {
			log_err("Failed to set non-blocking fd for serial port\n");
			return UFP_FAIL;
		}
	}

	dev->timeout_ms = config->timeout_ms;

	return UFP_OK;
}

ufprog_status UFPROG_API serial_port_get_config(serial_port dev, struct serial_port_config *retconfig)
{
	struct termios2 tio;
	struct termios tty;
	int rc, st, err;

	if (!dev || !retconfig)
		return UFP_INVALID_PARAMETER;

	rc = tcgetattr(dev->fd, &tty);
	if (rc) {
		err = errno;
		log_err("tcgetattr() failed with %d: %s\n", err, strerror(err));
		return UFP_FAIL;
	}

	memset(retconfig, 0, sizeof(*retconfig));

	if (tty.c_cflag & CS5)
		retconfig->data_bits = 5;
	else if (tty.c_cflag & CS6)
		retconfig->data_bits = 6;
	else if (tty.c_cflag & CS7)
		retconfig->data_bits = 7;
	else if (tty.c_cflag & CS8)
		retconfig->data_bits = 8;

	if (tty.c_cflag & CSTOPB)
		retconfig->stop_bits = SERIAL_STOP_BITS_2;
	else
		retconfig->stop_bits = SERIAL_STOP_BITS_1;

	if (tty.c_cflag & PARENB) {
		if (tty.c_cflag & PARODD) {
			if (tty.c_cflag & CMSPAR)
				retconfig->parity = SERIAL_PARITY_MARK;
			else
				retconfig->parity = SERIAL_PARITY_ODD;
		} else {
			if (tty.c_cflag & CMSPAR)
				retconfig->parity = SERIAL_PARITY_SPACE;
			else
				retconfig->parity = SERIAL_PARITY_EVEN;
		}
	} else {
		retconfig->parity = SERIAL_PARITY_NONE;
	}

	if (tty.c_cflag & CRTSCTS)
		retconfig->fc = SERIAL_FC_RTS_CTS;
	else if ((tty.c_iflag & (IXON | IXOFF)) == (IXON | IXOFF))
		retconfig->fc = SERIAL_FC_XON_XOFF;

	rc = ioctl(dev->fd, TIOCMGET, &st);
	if (rc < 0) {
		err = errno;
		log_err("ioctl() failed with %d: %s\n", err, strerror(err));
		return UFP_FAIL;
	}

	if (st & (TIOCM_DTR | TIOCM_DSR))
		retconfig->fc = SERIAL_FC_DTR_DSR;

	rc = ioctl(dev->fd, TCGETS2, &tio);
	if (rc < 0) {
		err = errno;
		log_err("ioctl() failed with %d: %s\n", err, strerror(err));
		return UFP_FAIL;
	}

	retconfig->baudrate = tio.c_ispeed;

	retconfig->timeout_ms = dev->timeout_ms;

	return UFP_OK;
}

ufprog_status UFPROG_API serial_port_flush(serial_port dev)
{
	int ret, err;

	if (!dev)
		return UFP_INVALID_PARAMETER;

	ret = tcflush(dev->fd, TCIOFLUSH);
	if (ret) {
		err = errno;
		log_err("tcflush() failed with %d: %s\n", err, strerror(err));
		return UFP_FAIL;
	}

	return UFP_OK;
}

static ufprog_status serial_port_read_once(serial_port dev, void *data, size_t len, size_t *retlen)
{
	struct timeval tv;
	ssize_t len_read;
	fd_set rdfs;
	int rc, err;

	*retlen = 0;

	if (dev->timeout_ms) {
		FD_ZERO(&rdfs);
		FD_SET(dev->fd, &rdfs);

		tv.tv_sec = dev->timeout_ms / 1000;
		tv.tv_usec = (dev->timeout_ms % 1000) * 1000;

		rc = select(dev->fd + 1, &rdfs, NULL, NULL, &tv);
		if (rc < 0) {
			err = errno;
			log_err("select() failed with %d: %s\n", err, strerror(err));
			return UFP_FAIL;
		}

		if (!rc) {
			log_err("Serial port read timed out\n");
			return UFP_TIMEOUT;
		}
	}

	len_read = read(dev->fd, data, len);
	if (len_read < 0) {
		err = errno;
		log_err("read() failed with %u: %s\n", err, strerror(err));
		return UFP_FAIL;
	}

	*retlen = len_read;

	return UFP_OK;
}

ufprog_status UFPROG_API serial_port_read(serial_port dev, void *data, size_t len, size_t *retlen)
{
	size_t read_len, chklen, total_len = 0;
	ufprog_status ret = UFP_OK;
	uint8_t *p = data;

	if (!dev || !data || !len)
		return UFP_INVALID_PARAMETER;

	while (len) {
		if (len > SSIZE_MAX)
			chklen = SSIZE_MAX;
		else
			chklen = len;

		ret = serial_port_read_once(dev, p, chklen, &read_len);

		total_len += read_len;
		len -= read_len;
		p += read_len;

		if (ret)
			break;
	}

	if (retlen)
		*retlen = total_len;

	return ret;
}

static ufprog_status serial_port_write_once(serial_port dev, const void *data, size_t len, size_t *retlen)
{
	struct timeval tv;
	ssize_t len_write;
	fd_set wrfs;
	int rc, err;

	*retlen = 0;

	if (dev->timeout_ms) {
		FD_ZERO(&wrfs);
		FD_SET(dev->fd, &wrfs);

		tv.tv_sec = dev->timeout_ms / 1000;
		tv.tv_usec = (dev->timeout_ms % 1000) * 1000;

		rc = select(dev->fd + 1, NULL, &wrfs, NULL, &tv);
		if (rc < 0) {
			err = errno;
			log_err("select() failed with %d: %s\n", err, strerror(err));
			return UFP_FAIL;
		}

		if (!rc) {
			log_err("Serial port write timed out\n");
			return UFP_TIMEOUT;
		}
	}

	len_write = write(dev->fd, data, len);
	if (len_write < 0) {
		err = errno;
		log_err("write() failed with %u: %s\n", err, strerror(err));
		return UFP_FAIL;
	}

	*retlen = len_write;

	return UFP_OK;
}

ufprog_status UFPROG_API serial_port_write(serial_port dev, const void *data, size_t len, size_t *retlen)
{
	size_t write_len, chklen, total_len = 0;
	ufprog_status ret = UFP_OK;
	const uint8_t *p = data;

	if (!dev || !data || !len)
		return UFP_INVALID_PARAMETER;

	while (len) {
		if (len > SSIZE_MAX)
			chklen = SSIZE_MAX;
		else
			chklen = len;

		ret = serial_port_write_once(dev, p, chklen, &write_len);

		total_len += write_len;
		len -= write_len;
		p += write_len;

		if (ret)
			break;
	}

	if (retlen)
		*retlen = total_len;

	return ret;
}
