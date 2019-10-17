// SPDX-License-Identifier: Apache-2.0
/*
 * Hostboot runtime interface
 *
 * Derived from src/include/runtime/interface.h in Hostboot
 *
 * Copyright 2013-2018 IBM Corp.
 */
#include <stdint.h>

#define HOSTBOOT_RUNTIME_INTERFACE_VERSION 0x9002

/** Memory error types defined for memory_error() interface. */
enum MemoryError_t
{
	/** Hardware has reported a solid memory CE that is
	 * correctable, but continues to report errors on subsequent
	 * reads. A second CE on that cache line will result in memory
	 * UE. Therefore, it is advised to migrate off of the address
	 * range as soon as possible. */
	MEMORY_ERROR_CE = 0,

	/** Hardware has reported an uncorrectable error in memory
	 * (memory UE, channel failure, etc). The hypervisor should
	 * migrate any partitions off this address range as soon as
	 * possible. Note that these kind of errors will most likely
	 * result in partition failures. It is advised that the
	 * hypervisor waits some time for PRD to handle hardware
	 * attentions so that the hypervisor will know all areas of
	 * memory that are impacted by the failure. */
	MEMORY_ERROR_UE = 1,

	/** Firmware has predictively requested service on a part in the memory
	 * subsystem. The partitions may not have been affected, but it is
	 * advised to migrate off of the address range as soon as possible to
	 * avoid potential partition outages. */
	MEMORY_ERROR_PREDICTIVE = 2,
};

/** Capability sets, for get_interface_capabilities */
#define HBRT_CAPS_SET0_COMMON		0
#define HBRT_CAPS_SET1_OPAL		1
#define HBRT_CAPS_SET2_PHYP		2

/* Capability flags */

/**
 * xscom_read and xscom_write return proper return codes on error.
 * Previous implementations may have incorrectly ignored failures.
 */
#define HBRT_CAPS_OPAL_HAS_XSCOM_RC   (1ul << 0)

/**
 * OPAL supports wakeup interface
 */
#define HBRT_CAPS_OPAL_HAS_WAKEUP_SUPPORT     (1ul << 1)

/**
 * OPAL supports '2=clear all previous forces' argument
 */
#define HBRT_CAPS_OPAL_HAS_WAKEUP_CLEAR       (1ul << 2)

/********************/


/**
 *  Load types for the load_pm_complex() interface
 *      HBRT_PM_LOAD: initial load of all lids/sections from scratch,
 *                    preserve nothing
 *      HBRT_PM_RELOAD: concurrent reload of all lids/sections,
 *                      but preserve runtime updates
 */
#define HBRT_PM_LOAD    0
#define HBRT_PM_RELOAD  1

/** Common return codes for scom_read(), scom_write(). */
#define HBRT_RC_RANGE__SCOM   0x1000

/* RC for a piberr is equal to 0x1000 plus the pib error value,
 made into a negative */
#define HBRT_RC_PIBERR_MASK            (0x00000000u - 0x00001007u) /* 0xFFFF_EFF9 */

#define HBRT_RC_PIBERR_001_BUSY        (0x00000000u - 0x00001001u) /* 0xFFFF_EFFF */
#define HBRT_RC_PIBERR_010_OFFLINE     (0x00000000u - 0x00001002u) /* 0xFFFF_EFFE */
#define HBRT_RC_PIBERR_011_PGOOD       (0x00000000u - 0x00001003u) /* 0xFFFF_EFFD */
#define HBRT_RC_PIBERR_100_INVALIDADDR (0x00000000u - 0x00001004u) /* 0xFFFF_EFFC */
#define HBRT_RC_PIBERR_101_CLOCKERR    (0x00000000u - 0x00001005u) /* 0xFFFF_EFFB */
#define HBRT_RC_PIBERR_110_PARITYERR   (0x00000000u - 0x00001006u) /* 0xFFFF_EFFA */
#define HBRT_RC_PIBERR_111_TIMEOUT     (0x00000000u - 0x00001007u) /* 0xFFFF_EFF9 */

/* Memory channel failure caused an error out to buffer chip. */
#define HBRT_RC_CHANNEL_FAILURE        (0x00000000u - 0x00001008u) /* 0xFFFF_EFF8 */

/* Any host-specific RCs will be this value or bigger */
#define HBRT_RC_NEXT_OPEN_RC           (0x00000000u - 0x00001009u) /* 0xFFFF_EFF7 */

/********************/


/** Common return codes for firmware_request(). -0x2000 */
#define HBRT_RC_RANGE__FIRMWARE_REQUEST   0x2000

/* FSP failed due to a a reset/reload. Only applicable when
 * hostInterfaces::hbrt_fw_msg::io_type is set to
 * HBRT_FW_MSG_HBRT_FSP_REQ
 */
#define HBRT_RC_FSPDEAD       -8193    //0x2001

/********************/


/** Common return codes for wakeup(). -0x3000 */
#define HBRT_RC_RANGE__WAKEUP   0x3000

/* Wakeup was rejected because core was in checkstop statte */
#define HBRT_RC_WAKEUP_INVALID_ON_CORE_XSTOP      -12289   /* -0x3001 */

/********************/


/* FSP failed due to a a reset/reload. Only applicable when
 * hostInterfaces::hbrt_fw_msg::io_type is set to
 * HBRT_FW_MSG_HBRT_FSP_REQ
 */
#define HBRT_RC_FSPDEAD       -8193    //0x2001

/********************/


/**
*  @brief Structure to be sent and received in the
*         firmware_request call
*
*  @note  see README.md file on how to create an HBRT to FW
*         request message interface
*/

enum  // hbrt_fw_msg::io_type             the struct associated with io_type
{
       HBRT_FW_MSG_TYPE_REQ_NOP           =  0,
       HBRT_FW_MSG_TYPE_RESP_NOP          =  1, // struct resp_generic
       HBRT_FW_MSG_TYPE_RESP_GENERIC      =  2, // struct resp_generic
       HBRT_FW_MSG_TYPE_REQ_HCODE_UPDATE  =  3, // struct req_hcode_update
       HBRT_FW_MSG_HBRT_FSP_REQ           =  4, // struct GenericFspMboxMessage_t
       HBRT_FW_MSG_TYPE_ERROR_LOG         =  5, // struct error_log
       HBRT_FW_MSG_HBRT_FSP_RESP          =  6, // struct GenericFspMboxMessage_t
       HBRT_FW_MSG_TYPE_I2C_LOCK          =  7, // struct req_i2c_lock
       HBRT_FW_MSG_TYPE_SBE_STATE         =  8, // struct sbe_state
       HBRT_FW_MSG_TYPE_NVDIMM_PROTECTION =  9, // struct nvdimm_protection_state
       HBRT_FW_MSG_TYPE_NVDIMM_OPERATION  = 10, // struct nvdimm_operation_t
       HBRT_FW_MSG_TYPE_GARD_EVENT        = 11, // struct gard_event_t
    };

// NVDIMM protection state enum
enum
{
        HBRT_FW_NVDIMM_NOT_PROTECTED = 0,
        HBRT_FW_NVDIMM_PROTECTED     = 1
};

// NVDIMM valid operations
// @note Multiple operations can be triggered at the same time using
//       a single NVDIMM operation call.  Having said that, operation
//       combinations should be sensical. Nonsensical operations will
//       error out.
enum NVDIMM_Op_t
{
        /// The following operations pertain to arming/disarming
        /// the NVDIMM
        // Disarm the NV logic such that the next save attempt is a NOOP
        HBRT_FW_NVDIMM_DISARM             = 0x0001,
        // Disable encryption on the NVDIMM and clear saved values from FW
        HBRT_FW_NVDIMM_DISABLE_ENCRYPTION = 0x0002,
        // Remove keys
        HBRT_FW_NVDIMM_REMOVE_KEYS        = 0x0004,
        // Enable encryption on the NVDIMM
        HBRT_FW_NVDIMM_ENABLE_ENCRYPTION  = 0x0008,
        // Arm the NV logic
        HBRT_FW_NVDIMM_ARM                = 0x0010,

        /// The following operations pertain to the Health of the NVDIMM
        /// This operation can be performed with the arming/disarming
        /// operation, these operation types are orthogonal to each other
        // Manufacturing(MNFG) energy source(ES) health check request
        HBRT_FW_MNFG_ES_HEALTH_CHECK      = 0x0020,
        // Manufacturing(MNFG) non-volatile memory(NVM) health check request
        HBRT_FW_MNFG_NVM_HEALTH_CHECK     = 0x0040
};

// NVDIMM (PHYP -> HBRT) message to request NVDIMM operation(s)
struct nvdimm_operation_t
{
        uint64_t    procId;  // Retrieve all NVDIMMs under the processor ID; all
                             // FFs (HBRT_NVDIMM_OPERATION_APPLY_TO_ALL_NVDIMMS)
                             // means operate on all NVDIMMs in the system
        uint32_t    rsvd1;   // reserved
        uint16_t    rsvd2;   // reserved
        uint16_t    opType;  // NVDIMM operation(s) to perform,
                             // see @note associated with NVDIMM_Op_t above
} __attribute__ ((packed));

// Gard event error type
// @note This needs to stay in sync with the FSP Mailbox specification for
//       command : Gard-able Error Detected - cmd 0xCE, s/c 0x63, mod 01
enum GARD_ERROR_t
{
    HBRT_GARD_ERROR_UNKNOWN                  = 0x0000,
    HBRT_GARD_ERROR_COMPUTATION_TEST_FAILURE = 0x0001,
    HBRT_GARD_ERROR_SLB                      = 0x0002,
    HBRT_GARD_ERROR_CHIP_TOD_FAILURE         = 0x0003,
    HBRT_GARD_ERROR_TIMEFAC_FAILURE          = 0x0004,
    HBRT_GARD_ERROR_PROC_RECOVERY_THRESHOLD  = 0x0005,
    HBRT_GARD_ERROR_NX                       = 0x0008,
    HBRT_GARD_ERROR_SLW                      = 0x0009,
    HBRT_GARD_ERROR_CAPP_UNIT                = 0x000A,

    // Mark the end of the gard error types.
    // This is not valid, just a marker
    HBRT_GARD_ERROR_LAST,
};

// Gard event (PHYP/OPAL -> HBRT)
struct gard_event_t
{
    uint32_t     i_error_type;     // Gard event error type enum
    uint32_t     i_procId;         // Processor ID for
                                   // error types 0x0001 to 0x0005
                                   // Chip ID for
                                   // error types 0x0008 to 0x000A
    uint32_t     i_plid;           // Platform log identifier
    uint16_t     i_sub_unit_mask;  // Currently not being used
    uint16_t     i_recovery_level; // Currently not being used
} __attribute__ ((packed));

struct hbrt_fw_msg   // define struct hbrt_fw_msg
{
   uint64_t io_type;          // message type from HBRT_FW_MSG_TYPE enum
   union
   {
      // This struct is returned from skiboot with
      // io_type set to HBRT_FW_MSG_TYPE_RESP_GENERIC or
      // with HBRT_FW_MSG_TYPE_RESP_NOP
      struct
      {
         uint64_t o_status;      // return code for a generic response
      } resp_generic;

      // This struct is sent from HBRT with
      // io_type set to HBRT_FW_MSG_TYPE_REQ_HCODE_UPDATE
      struct
      {
         uint64_t i_chipId;     // processor chip ID plus ID type,
                                // always proc (0x0)
         uint32_t i_section;    // runtime section to update
                                // (passthru to pore_gen_scom)
         uint32_t i_operation;  // type of operation to perform
                                // (passthru to pore_gen_scom)
         uint64_t i_scomAddr;   // fully qualified scom address
         uint64_t i_scomData;   // data for operation
      } req_hcode_update;

      // This struct is sent from HBRT with
      // io_type set to HBRT_FW_MSG_TYPE_ERROR_LOG
      // Send an error log to FSP
      struct
      {
         uint32_t i_plid;     // platform log identifier
         uint32_t i_errlSize; // data size in bytes
         uint8_t  i_data;     // the error log data
                              // uint8_t *myData =
                              // (uint8_t*)&l_req_fw_msg->error_log.i_data;
      } __attribute__ ((packed)) error_log;


      // This struct is sent from HBRT with
      // io_type set to HBRT_FW_MSG_TYPE_I2C_LOCK
      struct
      {
         uint64_t i_chipId;     // processor chip ID plus ID type,
                                // always proc (0x0)
         uint8_t i_i2cMaster;   // i2c master
                                // B=0, C=1, D=2, E=3
         uint8_t i_operation;   // type of operation to perform
                                // 1 = lock, 2 = unlock
      } __attribute__ ((packed)) req_i2c_lock;

      // This struct is sent from HBRT with
      // io_type set to  HBRT_FW_MSG_TYPE_SBE_STATE
      struct
      {
         uint64_t i_procId; // processor ID of the SBE that is disabled/enabled
         uint64_t i_state;  // state of the SBE; 0 = disabled, 1 = enabled
      } __attribute__ ((packed)) sbe_state;

      // This struct is sent from HBRT with
      // io_type set to HBRT_FW_MSG_TYPE_NVDIMM_PROTECTION
      struct
      {
         uint64_t i_procId; // processor ID of the NVDIMM with/without OCC protection
         uint64_t i_state;  // NVDIMM protection state enum
      } __attribute__ ((packed)) nvdimm_protection_state;

      // This struct is sent from PHYP to HBRT with
      // io_type set to HBRT_FW_MSG_TYPE_NVDIMM_OPERATION
      struct nvdimm_operation_t nvdimm_operation;

      // This struct is sent from PHYP/OPAL to HBRT with
      // io_type set to HBRT_FW_MSG_TYPE_GARD_EVENT
      struct gard_event_t gard_event;

      // This struct is sent from HBRT with
      // io_type set to HBRT_FW_MSG_HBRT_FSP_REQ or
      // HBRT_FW_MSG_HBRT_FSP_RESP
      // This struct sends/receives an MBox message to the FSP
//      struct GenericFspMboxMessage_t generic_msg;

   }; // end union
};  // end struct hbrt_fw_msg

struct host_interfaces {
	/** Interface version. */
	uint64_t interface_version;

	/** Put a string to the console. */
	void (*puts)(const char*);
	/** Critical failure in runtime execution. */
	void (*assert)(void);

	/** OPTIONAL. Hint to environment that the page may be executed. */
	int (*set_page_execute)(void*);

	/** malloc */
	void *(*malloc)(size_t);
	/** free */
	void (*free)(void*);
	/** realloc */
	void *(*realloc)(void*, size_t);

	/**
	 * @brief Send a PEL to the FSP
	 * @param[in] plid Platform Log identifier
	 * @param[in] data size in bytes
	 * @param[in] pointer to data
	 * @return 0 on success else error code
	 * @platform FSP
	 */
	int (*send_error_log)(uint32_t,uint32_t,void *);

	/**
	 * @brief Scan communication read
	 * @param[in] chip_id (based on devtree defn)
	 * @param[in] address
	 * @param[in] pointer to 8-byte data buffer
	 * @return 0 on success else return code
	 * @platform FSP,OpenPOWER
	 */
	int (*scom_read)(uint64_t, uint64_t, void*);

	/**
	 * @brief Scan communication write
	 * @param[in] chip_id (based on devtree defn)
	 * @param[in] address
	 * @param[in] pointer to 8-byte data buffer
	 * @return 0 on success else return code
	 * @platform FSP,OpenPOWER
	 */
	int (*scom_write)(uint64_t, uint64_t, const void *);

	/**
	 *  @brief Load a LID from PNOR, FSP, etc.
	 *
	 *  @param[in] LID number.
	 *  @param[out] Allocated buffer for LID.
	 *  @param[out] Size of LID (in bytes).
	 *
	 *  @return 0 on success, else RC.
	 *  @platform FSP
	 */
	int (*lid_load)(uint32_t lid, void **buf, size_t *len);

	/**
	 *  @brief Release memory from previously loaded LID.
	 *
	 *  @param[in] Allocated buffer for LID to release.
	 *
	 *  @return 0 on success, else RC.
	 *  @platform FSP
	 */
	int (*lid_unload)(void *buf);

	/**
	 *  @brief Get the address of a reserved memory region by its devtree
	 *  name.
	 *
	 *  @param[in] Devtree name (ex. "ibm,hbrt-vpd-image")
	 *  @param[in] Devtree instance
	 *  @return physical address of region (or NULL).
	 *  @platform FSP,OpenPOWER
	 */
	uint64_t (*get_reserved_mem)(const char *name, uint32_t instance);

	/**
	 * @brief  Force a core to be awake, or clear the force
	 * @param[in] i_core  Core to wake up (pid)
	 * @param[in] i_mode  0=force awake
	 *				1=clear force
	 *				2=clear all previous forces
	 * @return rc  non-zero on error
	 * @platform FSP
	 */
	int (*wakeup)( uint32_t i_core, uint32_t i_mode );

	/**
	 * @brief Delay/sleep for at least the time given
	 *
	 * The sleep time must be normalised; i_nano_seconds should be between
	 * 0 and 999999999.
	 *
	 * @param[in] seconds
	 * @param[in] nano seconds
	 * @platform FSP,OpenPOWER
	 */
	void (*nanosleep)(uint64_t i_seconds, uint64_t i_nano_seconds);

	/**
	 * @brief Report an OCC error to the host
	 * @param[in] Failing status that identifies the nature of the fail
	 * @param[in] Identifier that specifies the failing part
	 * @platform FSP
	 */
	void (*report_occ_failure)( uint64_t i_status, uint64_t i_partId );

	/**
	 *  @brief Reads the clock value from a POSIX clock.
	 *  @param[in]  i_clkId - The clock ID to read.
	 *  @param[out] o_tp - The timespec struct to store the clock value in.
	 *
	 *  @return 0 or -(errno).
	 *  @retval 0 - SUCCESS.
	 *  @retval -EINVAL - Invalid clock requested.
	 *  @retval -EFAULT - NULL ptr given for timespec struct.
	 *
	 * @platform OpenPOWER
	 */
	int (*clock_gettime)( clockid_t i_clkId, struct timespec* o_tp );

	/**
	 * @brief Read Pnor
	 * @param[in] i_proc: processor Id
	 * @param[in] i_partitionName: name of the partition to read
	 * @param[in] i_offset: offset within the partition
	 * @param[out] o_data: pointer to the data read
	 * @param[in] i_sizeBytes: size of data to read
	 * @retval rc - number of bytes read, or non-zero on error
	 * @platform OpenPOWER
	 */
	int (*pnor_read) ( uint32_t i_proc, const char* i_partitionName,
			uint64_t i_offset, void* o_data, size_t i_sizeBytes );

	/**
	 * @brief Write to Pnor
	 * @param[in] i_proc: processor Id
	 * @param[in] i_partitionName: name of the partition to write
	 * @param[in] i_offset: offset within the partition
	 * @param[in] i_data: pointer to the data to write
	 * @param[in] i_sizeBytes: size of data to write
	 * @retval rc - number of bytes written, or non-zero on error
	 * @platform OpenPOWER
	 */
	int (*pnor_write) ( uint32_t i_proc, const char* i_partitionName,
			uint64_t i_offset, void* i_data, size_t i_sizeBytes );


	/**
	 * i2c master description: chip, engine and port packed into
	 * a single 64-bit argument
	 *
	 * ---------------------------------------------------
	 * |         chip         |  reserved  |  eng | port |
         * |         (32)         |    (16)    |  (8) | (8)  |
	 * ---------------------------------------------------
	 */
#define HBRT_I2C_MASTER_CHIP_SHIFT	32
#define HBRT_I2C_MASTER_CHIP_MASK	(0xfffffffful << 32)
#define HBRT_I2C_MASTER_ENGINE_SHIFT	8
#define HBRT_I2C_MASTER_ENGINE_MASK	(0xfful << 8)
#define HBRT_I2C_MASTER_PORT_SHIFT	0
#define HBRT_I2C_MASTER_PORT_MASK	(0xfful)

	/**
	 * @brief Read data from an i2c device
	 * @param[in] i_master - Chip/engine/port of i2c bus
	 * @param[in] i_devAddr - I2C address of device
	 * @param[in] i_offsetSize - Length of offset (in bytes)
	 * @param[in] i_offset - Offset within device to read
	 * @param[in] i_length - Number of bytes to read
	 * @param[out] o_data - Data that was read
	 * @return 0 on success else return code
	 * @platform OpenPOWER
	 */
	int (*i2c_read)( uint64_t i_master, uint16_t i_devAddr,
			 uint32_t i_offsetSize, uint32_t i_offset,
			 uint32_t i_length, void* o_data );

	/**
	 * @brief Write data to an i2c device
	 * @param[in] i_master - Chip/engine/port of i2c bus
	 * @param[in] i_devAddr - I2C address of device
	 * @param[in] i_offsetSize - Length of offset (in bytes)
	 * @param[in] i_offset - Offset within device to write
	 * @param[in] i_length - Number of bytes to write
	 * @param[in] Data to write
	 * @return 0 on success else return code
	 * @platform OpenPOWER
	 */
	int (*i2c_write)( uint64_t i_master, uint16_t i_devAddr,
			  uint32_t i_offsetSize, uint32_t i_offset,
			  uint32_t i_length, void* i_data );

	/**
	 * Perform an IPMI transaction
	 * @param[in] netfn The IPMI netfn byte
	 * @param[in] cmd The IPMI cmd byte
	 * @param[in] tx_buf The IPMI packet to send to the host
	 * @param[in] tx_size The number of bytes, to send
	 * @param[in] rx_buf A buffer to be populated with the IPMI
	 *		response.
	 * @param[inout] rx_size The allocated size of the rx buffer on
	 *		input, updated to the size of the response on output.
	 *		This should always begin with the IPMI completion
	 *		code.
	 */
	int (*ipmi_msg)(uint8_t netfn, uint8_t cmd,
			void *tx_buf, size_t tx_size,
			void *rx_buf, size_t *rx_size);


	/**
	 * @brief Hardware has reported a memory error. This function requests
	 * the hypervisor to remove the all addresses within the address range
	 * given (including endpoints) from the available memory space.
	 *
	 * It is understood that the hypervisor may not be able to immediately
	 * deallocate the memory because it may be in use by a partition.
	 * Therefore, the hypervisor should cache all requests and deallocate
	 * the memory once it has been freed.
	 *
	 * @param  i_startAddr The beginning address of the range.
	 * @param  i_endAddr   The end address of the range.
	 * @param  i_errorType See enum MemoryError_t.
	 *
	 * @return 0 if the request is successfully received. Any value other
	 *	than 0 on failure. The hypervisor should cache the request and
	 *	return immediately. It should not wait for the request to be
	 *	applied. See note above.
	 */
	int (*memory_error)( uint64_t i_startAddr, uint64_t i_endAddr,
					  enum MemoryError_t i_errorType );

	/**
	 * @brief Query the prd infrastructure for interface capabilities.
	 * @param[in] i_set The set of capabilites to retrieve
	 *
	 * @return a bitmask containing the relevant HBRT_CAPS_* for
	 *	this implementation and the specified set.
	 */
	uint64_t (*get_interface_capabilities)(uint64_t i_set);

	/**
	 *  @brief Map a physical address space into usable memory
	 *  @note Repeated calls to map the same memory should not return an
	 *        error
	 *  @param[in]  i_physMem  Physical address
	 *  @param[in]  i_bytes    Number of bytes to map in
	 *  @return NULL on error, else pointer to usable memory
	 *  @platform FSP, OpenPOWER
	 */
	void* (*map_phys_mem)(uint64_t i_physMem, size_t i_bytes);

	/**
	 *  @brief Unmap a physical address space from usable memory
	 *  @param[in]  i_ptr  Previously mapped pointer
	 *  @return 0 on success, else RC
	 *  @platform FSP, OpenPOWER
	 */
	int (*unmap_phys_mem)(void* i_ptr);

	/**
	 *  @brief Modify the SCOM restore section of the HCODE image with the
	 *         given register data
	 *
	 *  @note The Hypervisor should perform the following actions:
	 *        - insert the data into the HCODE image (p9_stop_api)
	 *
	 *  @pre HBRT is responsible for enabling special wakeup on the
	 *       associated core(s) before calling this interface
	 *
	 *  @param  i_chipId    processor chip ID
	 *                       plus ID type, always proc (0x0)
	 *  @param  i_section   runtime section to update
	 *                      (passthru to pore_gen_scom)
	 *  @param  i_operation type of operation to perform
	 *                      (passthru to pore_gen_scom)
	 *  @param  i_scomAddr  fully qualified scom address
	 *  @param  i_scomData  data for operation
	 *
	 *  @return 0 if the request is successfully received.
	 *          Any value other than 0 on failure.
	 *  @platform FSP, OpenPOWER
	 */
	int (*hcode_scom_update)(uint64_t i_chipId,
			uint32_t i_section,
			uint32_t i_operation,
			uint64_t i_scomAddr,
			uint64_t i_scomData);

	/**
	 * @brief Send a request to firmware, and receive a response
	 * @details
	 *   req_len bytes are sent to runtime firmware, and resp_len
	 *   bytes received in response.
	 *
	 *   Both req and resp are allocated by the caller. If resp_len
	 *   is not large enough to contain the full response, an error
	 *   is returned.
	 *
	 * @param[in]  i_reqLen       length of request data
	 * @param[in]  i_req          request data
	 * @param[inout] o_respLen    in: size of request data buffer
	 *                            out: length of request data
	 * @param[in]  o_resp         response data
	 * @return 0 on success, else RC
	 * @platform FSP, OpenPOWER
	 */
	int (*firmware_request)(uint64_t i_reqLen, void *i_req,
			uint64_t *o_respLen, void *o_resp);

	/* Reserve some space for future growth. */
	void (*reserved[27])(void);
};

struct runtime_interfaces {
	/** Interface version. */
	uint64_t interface_version;

	/**
	 * @brief Execute CxxTests that may be contained in the image.
	 *
	 * @param[in] - Pointer to CxxTestStats structure for results reporting.
	 */
	void (*cxxtestExecute)(void *);

	/**
	 * @brief Get a list of lids numbers of the lids known to HostBoot
	 *
	 * @param[out] o_num - the number of lids in the list
	 * @return a pointer to the list
	 * @platform FSP
	 */
	const uint32_t * (*get_lid_list)(size_t * o_num);

	/**
	 * @brief Load OCC Image and common data into mainstore, also setup OCC
	 * BARSs
	 *
	 * @param[in] i_homer_addr_phys - The physical mainstore address of the
	 *	start of the HOMER image
	 * @param[in] i_homer_addr_va - Virtual memory address of the HOMER
	 *	image
	 * @param[in] i_common_addr_phys - The physical mainstore address
	 *	of the OCC common area.
	 * @param[in] i_common_addr_va - Virtual memory address of the common
	 *	area
	 * @param[in] i_chip - The HW chip id (XSCOM chip ID)
	 * @return 0 on success else return code
	 * @platform FSP
	 */
	int (*occ_load)(uint64_t i_homer_addr_phys,
			 uint64_t i_homer_addr_va,
			 uint64_t i_common_addr_phys,
			 uint64_t i_common_addr_va,
			 uint64_t i_chip);

	/**
	 * @brief Start OCC on all chips, by module
	 *
	 *  @param[in] i_chip - Array of functional HW chip ids
	 *  @Note The caller must include a complete modules worth of chips
	 *  @param[in] i_num_chips - Number of chips in the array
	 *  @return 0 on success else return code
	 *  @platform FSP
	 */
	int (*occ_start)(uint64_t* i_chip, size_t i_num_chips);

	/**
	 * @brief Stop OCC hold OCCs in reset
	 *
	 *  @param[in] i_chip - Array of functional HW chip ids
	 *  @Note The caller must include a complete modules worth of chips
	 *  @param[in] i_num_chips - Number of chips in the array
	 *  @return 0 on success else return code
	 *  @platform FSP
	 */
	int (*occ_stop)(uint64_t* i_chip, size_t i_num_chips);

	/**
	 * @brief Notify HTMGT that an OCC has an error to report
	 *
	 * @details  When an OCC has encountered an error that it wants to
	 *		   be reported, this interface will be called to trigger
	 *		   HTMGT to collect and commit the error.
	 *
	 * @param[i] i_chipId - Id of processor with failing OCC
	 * @platform OpenPower
	 */
	void (*process_occ_error) (uint64_t i_chipId);

	/**
	 *  @brief Enable chip attentions
	 *
	 *  @return 0 on success else return code
	 *  @platform OpenPower
	 */
	int (*enable_attns)(void);

	/**
	 *  @brief Disable chip attentions
	 *
	 *  @return 0 on success else return code
	 *  @platform OpenPower
	 */
	int (*disable_attns)(void);

	/**
	 *  @brief handle chip attentions
	 *
	 *  @param[in] i_proc - processor chip id at attention XSCOM chip id
	 *	based on devtree defn
	 *  @param[in] i_ipollStatus - processor chip Ipoll status
	 *  @param[in] i_ipollMask   - processor chip Ipoll mask
	 *  @return 0 on success else return code
	 *  @platform OpenPower
	 */
	int (*handle_attns)(uint64_t i_proc, uint64_t i_ipollStatus,
			uint64_t i_ipollMask);

	/**
	 * @brief Notify HTMGT that an OCC has failed and needs to be reset
	 *
	 * @details  When BMC detects an OCC failure that requires a reset,
	 * this interface will be called to trigger the OCC reset.  HTMGT
	 * maintains a reset count and if there are additional resets
	 * available, the OCCs get reset/reloaded.  If the recovery attempts
	 * have been exhauseted or the OCC fails to go active, an unrecoverable
	 * error will be logged and the system will remain in safe mode.
	 *
	 * @param[in]  i_chipId  ChipID which identifies the OCC reporting an
	 *	error
	 * @platform OpenPOWER
	 */
	void (*process_occ_reset)(uint64_t  i_chipId);

	/**
	 * @brief Change the OCC state
	 *
	 * @details  This is a blocking call that will change the OCC state.
	 * The OCCs will only actuate (update processor frequency/ voltages)
	 * when in Active state.  The OCC will only be monitoring/observing
	 * when in Observation state.
	 *
	 * @note When the OCCs are initially started, the state will
	 * default to Active.  If the state is changed to Observation, that
	 * state will be retained until the next IPL. (If the OCC would get
	 * reset, it would return to the last requested state)
	 *
	 * @param[in]  i_occActivation  set to true to move OCC to Active state
	 *	or false to move OCC to Observation state
	 *
	 * @return  0 on success, or return code if the state did not change.
	 * @platform OpenPower
	 */
	int (*enable_occ_actuation)(bool i_occActivation);

	/**
	 * @brief	Apply a set of attribute overrides
	 *
	 * @param[in]	pointer to binary override data
	 * @param[in]	length of override data (bytes)
	 * @returns	0 on success, or return code if the command failed
	 *
	 * @platform	OpenPower
	 */
	int (*apply_attr_override)(uint8_t *i_data, size_t size);

	/**
	 * @brief	Send a pass-through command to HTMGT
	 *
	 * @details	This is a blocking call that will send a command to
	 *		HTMGT.
	 *
	 * @note	If o_rspLength is returned with a non-zero value, the
	 *		data at the o_rspData should be dumped to stdout in a
	 *		hex dump format.
	 * @note	The maximum response data returned will be 4096 bytes
	 *
	 * @param[in]	i_cmdLength	number of bytes in pass-thru command data
	 * @param[in]	*i_cmdData	pointer to pass-thru command data
	 * @param[out]	*o_rspLength	pointer to number of bytes returned in
	 *				o_rspData
	 * @param[out]	*o_rspData	pointer to a 4096 byte buffer that will
	 *				contain the response data from the command
	 *
	 * @returns	0 on success, or return code if the command failed
	 * @platform	OpenPower
	 */
	int (*mfg_htmgt_pass_thru)(uint16_t i_cmdLength, uint8_t *i_cmdData,
				   uint16_t *o_rspLength, uint8_t *o_rspData);

	/**
	 * @brief	Execute an arbitrary command inside hostboot runtime
	 * @param[in]	Number of arguments (standard C args)
	 * @param[in]	Array of argument values (standard C args)
	 * @param[out]	Response message (NULL terminated), memory allocated
	 *		by hbrt, if o_outString is NULL then no response will
	 *		be sent
	 * @return	0 on success, else error code
	 */
	int (*run_command)(int argc, const char **argv, char **o_outString);

	/**
	 *  @brief Verify integrity of a secure container
	 *  @param[in] i_pContainer Pointer to a valid secure container,
	 *      Must not be NULL.  Container is assumed to be stripped of any
	 *      ECC and must start with a valid secure header (which contains
	 *      the container size information)
	 *  @param[in] i_pHwKeyHash Pointer to a valid hardware keys' hash.
	 *      Must not be NULL.
	 *  @param[in] i_hwKeyHashSize Size of the hardware keys' hash.
	 *      A value which incorrectly states the size of the hardware keys'
	 *      hash will be detected as a verification error or worse, an
	 *      illegal memory access.  Must not be 0.
	 *  @note If secureboot is compiled out, the function pointer will be
	 *      set to NULL.  If caller's secureboot support is compiled in and
	 *      secureboot is enabled by policy, then caller should treat a NULL
	 *      pointer as a verification failure.
	 *  @return Integer error code indicating success or failure
	 *  @retval 0 Container verified correctly
	 *  @retval !0 API error or otherwise failed to verify container
	 *  @platform FSP, OpenPOWER
	 */
	int (*verify_container)(const void *i_pContainer,
			const void *i_pHwKeyHash,
			size_t i_hwKeyHashSize);

	/**
	 *  @brief SBE message passing
	 *
	 *  @details
	 *      This is a blocking call that will pass an SBE message
	 *      with a pass-through command through HBRT to code that
	 *      will process the command and provide a response.
	 *
	 *  @param[in] i_procChipId Chip ID of the processor whose SBE is
	 *      passing the message and sent the interrupt
	 *
	 *  @return 0 on success, or return code if the command failed
	 *  @platform FSP, OpenPOWER
	 */
	int (*sbe_message_passing)(uint32_t i_procChipId);

	/**
	 *  @brief Load OCC/HCODE images into mainstore
	 *
	 *  @param[in] i_chip            the HW chip id (XSCOM chip ID)
	 *  @param[in] i_homer_addr      the physical mainstore address of the
	 *                               start of the HOMER image,
	 *  @param[in] i_occ_common_addr the physical mainstore address of the
	 *                               OCC common area, 8MB, used for
	 *                               OCC-OCC communication (1 per node)
	 *  @param[in] i_mode            selects initial load vs concurrent
	 *                               reloads
	 *                               HBRT_PM_LOAD:
	 *                                  load all lids/sections from scratch,
	 *                                  preserve nothing
	 *                               HBRT_PM_RELOAD:
	 *                                  reload all lids/sections,
	 *                                  but preserve runtime updates
	 *  @return 0 on success else return code
	 *  @platform FSP, OpenPOWER
	 */
	int (*load_pm_complex)(uint64_t i_chip,
			uint64_t i_homer_addr,
			uint64_t i_occ_common_addr,
			uint32_t i_mode);

	/**
	 *  @brief Start OCC/HCODE on the specified chip
	 *  @param[in] i_chip the HW chip id
	 *  @return 0 on success else return code
	 *  @platform FSP, OpenPOWER
	 */
	int (*start_pm_complex)(uint64_t i_chip);

	/**
	 *  @brief Reset OCC/HCODE on the specified chip
	 *  @param[in] i_chip the HW chip id
	 *  @return 0 on success else return code
	 *  @platform FSP, OpenPOWER
	 */
	int (*reset_pm_complex)(uint64_t i_chip);

	/**
	 * @brief Query the IPOLL event mask supported by HBRT
	 *
	 * @details  This call allows the wrapper application to query
	 * the ipoll event mask to set when the HBRT instance is running. Bits
	 * that are *set* in this bitmask represent events that will be
	 * forwarded to the handle_attn() callback.
	 *
	 * @return        The IPOLL event bits to enable during HBRT execution
	 * @platform FSP, OpenPOWER
	 */
	uint64_t (*get_ipoll_events)(void);

	/**
	 * @brief Receive an async notification from firmware
	 * @param[in] i_len   length of notification data
	 * @param[in] i_data  notification data
	 * @platform FSP, OpenPOWER
	 */
	void (*firmware_notify)(uint64_t len, void *data);

        /**
         *  @brief Prepare for HBRT concurrent code update
         *
         *  @details  This call allows the Host to inform HBRT that a concurrent
         *  code update has been initiated.  HBRT then prepares updated targeting
         *  data for use by the updated HBRT code.
         *
         *  @return        0 on success else return code
         *  @platform FSP
         */
        int (*prepare_hbrt_update)( void );


	/* Reserve some space for future growth. */
	void (*reserved[21])(void);
};
