// license:BSD-3-Clause
// copyright-holders:R. Belmont
/***************************************************************************

  nubus_image.c - synthetic NuBus card to allow reading/writing "raw"
  HFS images, including floppy images (DD and HD) and vMac/Basilisk HDD
  volumes up to 256 MB in size.

***************************************************************************/

#include "emu.h"
#include "nubus_image.h"
#include "osdcore.h"
#include "uiinput.h"
#include "ui/ui.h"
#include "ui/menu.h"
#include "ui/filemngr.h"
#include "ui/filesel.h"
#include <unistd.h>
#include <sys/fcntl.h>
#include <sys/stat.h>

extern "C" int SDL_ShowCursor(int toggle);

#define IMAGE_ROM_REGION    "image_rom"
#define IMAGE_DISK0_TAG     "nb_disk"

#define MESSIMG_DISK_SECTOR_SIZE (512)


// nubus_image_device::messimg_disk_image_device

class nubus_image_device::messimg_disk_image_device :   public device_t,
								public device_image_interface
{
public:
	// construction/destruction
	messimg_disk_image_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock);

	// image-level overrides
	virtual iodevice_t image_type() const override { return IO_QUICKLOAD; }

	virtual bool is_readable()  const override { return 1; }
	virtual bool is_writeable() const override { return 1; }
	virtual bool is_creatable() const override { return 0; }
	virtual bool must_be_loaded() const override { return 0; }
	virtual bool is_reset_on_load() const override { return 0; }
	virtual const char *file_extensions() const override { return "img"; }
	virtual const char *custom_instance_name() const override { return "disk"; }
	virtual const char *custom_brief_instance_name() const override { return "disk"; }

	virtual image_init_result call_load() override;
	virtual void call_unload() override;

	protected:
	// device-level overrides
	virtual void device_start() override;
	virtual void device_reset() override;
public:
	uint32_t m_size;
	std::unique_ptr<uint8_t[]> m_data;
	bool m_ejected;
};


// device type definition
DEFINE_DEVICE_TYPE_NS(MESSIMG_DISK, nubus_image_device, messimg_disk_image_device, "messimg_disk_image", "Mac image")

nubus_image_device::messimg_disk_image_device::messimg_disk_image_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock) :
	device_t(mconfig, MESSIMG_DISK, tag, owner, clock),
	device_image_interface(mconfig, *this),
	m_size(0), m_data(nullptr), m_ejected(false)
{
}


/*-------------------------------------------------
    device start callback
-------------------------------------------------*/

void nubus_image_device::messimg_disk_image_device::device_start()
{
	m_data = nullptr;

	if (exists() && fseek(0, SEEK_END) == 0)
	{
		m_size = (uint32_t)ftell();
	}
}

image_init_result nubus_image_device::messimg_disk_image_device::call_load()
{
	fseek(0, SEEK_END);
	m_size = (uint32_t)ftell();
	if (m_size > (256*1024*1024))
	{
		osd_printf_error("Mac image too large: must be 256MB or less!\n");
		m_size = 0;
		return image_init_result::FAIL;
	}

	m_data = make_unique_clear<uint8_t[]>(m_size);
	fseek(0, SEEK_SET);
	fread(m_data.get(), m_size);
	m_ejected = false;

	return image_init_result::PASS;
}

void nubus_image_device::messimg_disk_image_device::call_unload()
{
	// TODO: track dirty sectors and only write those
	fseek(0, SEEK_SET);
	fwrite(m_data.get(), m_size);
	m_size = 0;
	//free(m_data);
}

/*-------------------------------------------------
    device reset callback
-------------------------------------------------*/

void nubus_image_device::messimg_disk_image_device::device_reset()
{
}

ROM_START( image )
	ROM_REGION(0x2000, IMAGE_ROM_REGION, 0)
	ROM_LOAD( "nb_fake.bin",  0x000000, 0x002000, CRC(9264bac5) SHA1(540c2ce3c90382b2da6e1e21182cdf8fc3f0c930) )
ROM_END

//**************************************************************************
//  GLOBAL VARIABLES
//**************************************************************************

DEFINE_DEVICE_TYPE(NUBUS_IMAGE, nubus_image_device, "nb_image", "NuBus Disk Image Pseudo-Card")


//-------------------------------------------------
//  device_add_mconfig - add device configuration
//-------------------------------------------------

MACHINE_CONFIG_START(nubus_image_device::device_add_mconfig)
	MCFG_DEVICE_ADD(IMAGE_DISK0_TAG, MESSIMG_DISK, 0)
MACHINE_CONFIG_END

//-------------------------------------------------
//  rom_region - device-specific ROM region
//-------------------------------------------------

const tiny_rom_entry *nubus_image_device::device_rom_region() const
{
	return ROM_NAME( image );
}

//**************************************************************************
//  LIVE DEVICE
//**************************************************************************

//-------------------------------------------------
//  nubus_image_device - constructor
//-------------------------------------------------

nubus_image_device::nubus_image_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock) :
	nubus_image_device(mconfig, NUBUS_IMAGE, tag, owner, clock)
{
}

nubus_image_device::nubus_image_device(const machine_config &mconfig, device_type type, const char *tag, device_t *owner, uint32_t clock) :
	device_t(mconfig, type, tag, owner, clock),
	device_nubus_card_interface(mconfig, *this),
	m_image(nullptr)
{
}

//-------------------------------------------------
//  device_start - device-specific startup
//-------------------------------------------------

void nubus_image_device::device_start()
{
	uint32_t slotspace;
	uint32_t superslotspace;

	install_declaration_rom(this, IMAGE_ROM_REGION);

	slotspace = get_slotspace();
	superslotspace = get_super_slotspace();

//  printf("[image %p] slotspace = %x, super = %x\n", this, slotspace, superslotspace);

	nubus().install_device(slotspace, slotspace+3, read32_delegate(FUNC(nubus_image_device::image_r), this), write32_delegate(FUNC(nubus_image_device::image_w), this));
	nubus().install_device(slotspace+4, slotspace+7, read32_delegate(FUNC(nubus_image_device::image_status_r), this), write32_delegate(FUNC(nubus_image_device::image_status_w), this));
	nubus().install_device(slotspace+8, slotspace+11, read32_delegate(FUNC(nubus_image_device::file_cmd_r), this), write32_delegate(FUNC(nubus_image_device::file_cmd_w), this));
	nubus().install_device(slotspace+12, slotspace+15, read32_delegate(FUNC(nubus_image_device::file_data_r), this), write32_delegate(FUNC(nubus_image_device::file_data_w), this));
	nubus().install_device(slotspace+16, slotspace+19, read32_delegate(FUNC(nubus_image_device::file_len_r), this), write32_delegate(FUNC(nubus_image_device::file_len_w), this));
	nubus().install_device(slotspace+20, slotspace+147, read32_delegate(FUNC(nubus_image_device::file_name_r), this), write32_delegate(FUNC(nubus_image_device::file_name_w), this));
	nubus().install_device(slotspace+148, slotspace+151, read32_delegate(FUNC(nubus_image_device::mousepos_r), this), write32_delegate(FUNC(nubus_image_device::mousepos_w), this));
	nubus().install_device(superslotspace, superslotspace+((256*1024*1024)-1), read32_delegate(FUNC(nubus_image_device::image_super_r), this), write32_delegate(FUNC(nubus_image_device::image_super_w), this));

	m_image = subdevice<messimg_disk_image_device>(IMAGE_DISK0_TAG);

	filectx.curdir[0] = '.';
	filectx.curdir[1] = '\0';
	memset(filectx.filename, 0, sizeof(filectx.filename));
	filectx.filenameoffset = 0;
	filectx.dirp = nullptr;
	filectx.fd = nullptr;

	importctx.dir = ".";
	importctx.file = "";
	importctx.result = (int)ui::menu_file_selector::result::INVALID;
	importctx.fd = nullptr;
}

//-------------------------------------------------
//  device_reset - device-specific reset
//-------------------------------------------------

void nubus_image_device::device_reset()
{
}

WRITE32_MEMBER( nubus_image_device::mousepos_w )
{
	if(data) {
		SDL_ShowCursor(1);
	} else {
		SDL_ShowCursor(0);
	}
}

READ32_MEMBER( nubus_image_device::mousepos_r )
{
	std::int32_t x, y;
	std::uint32_t ret = 0;
	bool m_mouse_button;
	// There doesn't seem to be a way to explicitly hide
	// the cursor using mame classes.
	machine().ui_input().find_mouse(&x, &y, &m_mouse_button);
	ret = ((x&0xffff) << 16) | (y&0xffff);

	return ret;
}

WRITE32_MEMBER( nubus_image_device::image_status_w )
{
	m_image->m_ejected = true;
}

READ32_MEMBER( nubus_image_device::image_status_r )
{
	if(m_image->m_ejected) {
		return 0;
	}

	if(m_image->m_size) {
		return 1;
	}
	return 0;
}

WRITE32_MEMBER( nubus_image_device::image_w )
{
}

READ32_MEMBER( nubus_image_device::image_r )
{
	return m_image->m_size;
}

WRITE32_MEMBER( nubus_image_device::image_super_w )
{
	uint32_t *image = (uint32_t*)m_image->m_data.get();
	data = ((data & 0xff) << 24) | ((data & 0xff00) << 8) | ((data & 0xff0000) >> 8) | ((data & 0xff000000) >> 24);
	mem_mask = ((mem_mask & 0xff) << 24) | ((mem_mask & 0xff00) << 8) | ((mem_mask & 0xff0000) >> 8) | ((mem_mask & 0xff000000) >> 24);

	COMBINE_DATA(&image[offset]);
}

READ32_MEMBER( nubus_image_device::image_super_r )
{
	uint32_t *image = (uint32_t*)m_image->m_data.get();
	uint32_t data = image[offset];
	return ((data & 0xff) << 24) | ((data & 0xff00) << 8) | ((data & 0xff0000) >> 8) | ((data & 0xff000000) >> 24);
}

WRITE32_MEMBER( nubus_image_device::file_cmd_w )
{
	filectx.curcmd = data;
	switch(data) {
	case kFileCmdGetDir:
	case kFileCmdSetDir:
	case kFileCmdGetFirstListing:
	case kFileCmdGetNextListing:
		break;
	case kFileCmdGetFile:
		{
			std::string fullpath;
			fullpath.reserve(1024);
			fullpath.assign((const char *)filectx.curdir);
			fullpath.append(PATH_SEPARATOR);
			fullpath.append((const char*)filectx.filename);
			if(osd_file::open(fullpath, OPEN_FLAG_READ, filectx.fd, filectx.filelen) != osd_file::error::NONE)
				osd_printf_error("Error opening %s\n", fullpath.c_str());
			filectx.bytecount = 0;
		}
		break;
	case kFileCmdPutFile:
		{
			std::string fullpath;
			fullpath.reserve(1024);
			fullpath.assign((const char *)filectx.curdir);
			fullpath.append(PATH_SEPARATOR);
			fullpath.append((const char*)filectx.filename);
			uint64_t filesize; // unused, but it's an output from the open call
			if(osd_file::open(fullpath, OPEN_FLAG_WRITE|OPEN_FLAG_CREATE, filectx.fd, filesize) != osd_file::error::NONE)
				osd_printf_error("Error opening %s\n", fullpath.c_str());
			filectx.bytecount = 0;
		}
		break;
	case kFileCmdFileClose:
		filectx.filenameoffset = 0;
		if(filectx.fd) {
			filectx.fd->flush();
		}
		filectx.fd = nullptr;
		break;
	case kFileCmdImportUI:
		importctx.result = 0;
		ui::menu::stack_reset(machine());
		ui::menu::stack_push<ui::menu_file_selector>(dynamic_cast<mame_ui_manager&>(machine().ui()), machine().render().ui_container(), nullptr, importctx.dir, importctx.file, false, false, false, (ui::menu_file_selector::result&)importctx.result);
		dynamic_cast<mame_ui_manager&>(machine().ui()).show_menu();
		break;
	case kFileCmdImportClose:
		importctx.fd = nullptr;
		break;
	default:
		printf("Unknown command: %x\n", data);
	}

	lastcmd = data;
}

READ32_MEMBER( nubus_image_device::file_cmd_r )
{
	if(lastcmd == kFileCmdImportUI) {
		uint32_t ret = importctx.result;
		if(importctx.result == (int)ui::menu_file_selector::result::FILE) {
			importctx.name_offset = 0;
			importctx.bytecount = 0;
			if(osd_file::open(importctx.file, OPEN_FLAG_READ, importctx.fd, importctx.filelen) != osd_file::error::NONE) {
				printf("Error opening %s\n", importctx.file.c_str());
			}
		} else if(!machine().ui().is_menu_active()) {
			ret = 0xffffffff;
		}
		return ret;
	}
	return 0;
}

WRITE32_MEMBER( nubus_image_device::file_data_w )
{
	std::uint32_t count = 4;
	std::uint32_t actualcount = 0;

	if(filectx.fd) {
		data = big_endianize_int32(data);
		if((filectx.bytecount + count) > filectx.filelen) count = filectx.filelen - filectx.bytecount;
		filectx.fd->write(&data, filectx.bytecount, count, actualcount);
		filectx.bytecount += actualcount;

		if(filectx.bytecount >= filectx.filelen) {
			filectx.fd.reset();
		}
	}
}

READ32_MEMBER( nubus_image_device::file_data_r )
{
	if(importctx.fd) {
		std::uint32_t ret;
		std::uint32_t actual = 0;
		importctx.fd->read(&ret, importctx.bytecount, sizeof(ret), actual);
		importctx.bytecount += actual;
		if(actual < sizeof(ret)) {
			importctx.fd.reset();
		}
		return big_endianize_int32(ret);
	}
	return 0;
}

WRITE32_MEMBER( nubus_image_device::file_len_w )
{
	filectx.filelen = data;
}

READ32_MEMBER( nubus_image_device::file_len_r )
{
	return importctx.filelen;
}

WRITE32_MEMBER( nubus_image_device::file_name_w )
{
	filectx.filename[filectx.filenameoffset] = data;
	filectx.filenameoffset++;
}

READ32_MEMBER( nubus_image_device::file_name_r )
{
	uint32_t ret = 0;
	if(lastcmd == 8) {
		if(importctx.name_offset < (importctx.file.length() - importctx.dir.length() - 1)) {
			ret = importctx.file.c_str()[importctx.name_offset + importctx.dir.length()+1];
			importctx.name_offset++;
		} else {
			ret = 0; // nul
		}
	}
	return ret;
}
