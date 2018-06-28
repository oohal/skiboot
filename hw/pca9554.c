#include <skiboot.h>
#include <opal.h>
#include <gpio.h>
#include <i2c.h>

int64_t pca9554_reg_read(struct i2c_bus *bus, int addr, int reg, uint8_t *v)
{
	return i2c_request_send(bus, addr, SMBUS_READ, reg, 1, v, 1, 0);
}

int64_t pca9554_reg_write(struct i2c_bus *bus, int addr, int reg, uint8_t v)
{
	uint8_t val = v;

	return i2c_request_send(bus, addr, SMBUS_WRITE, reg, 1, &val, 1, 0);
}

/*
 * Does a Read-Modify-Write to change the bits in @mask in the register @reg.
 */
static int64_t pca9554_change_bits(struct i2c_bus *bus, int addr, int reg,
			uint8_t mask, bool set)
{
	uint8_t val;
	int64_t rc;

	if (!mask)
		return OPAL_SUCCESS;

	rc = pca9554_reg_read(bus, addr, reg, &val);
	if (rc)
		return rc;

	if (set)
		val |= mask;
	else
		val &= ~mask;

	rc = pca9554_reg_write(bus, addr, reg, val);
	if (rc)
		return rc;

	return OPAL_SUCCESS;
}

int64_t pca9554_set_bits(struct i2c_bus *bus, int addr, int reg, uint8_t bits)
{
	return pca9554_change_bits(bus, addr, reg, bits, true);
}

int64_t pca9554_clr_bits(struct i2c_bus *bus, int addr, int reg, uint8_t bits)
{
	return pca9554_change_bits(bus, addr, reg, bits, false);
}
