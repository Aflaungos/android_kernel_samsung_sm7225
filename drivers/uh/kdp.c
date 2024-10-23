#include <asm-generic/sections.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/slub_def.h>

#include <linux/kdp.h>
#include <linux/cred.h>
#include <linux/security.h>
#include <linux/init_task.h>
#include <../../fs/mount.h>

/* security/selinux/include/objsec.h */
struct task_security_struct {
	u32 osid;               /* SID prior to last execve */
	u32 sid;                /* current SID */
	u32 exec_sid;           /* exec SID */
	u32 create_sid;         /* fscreate SID */
	u32 keycreate_sid;      /* keycreate SID */
	u32 sockcreate_sid;     /* fscreate SID */
	void *bp_cred;
};
/* security/selinux/hooks.c */
struct task_security_struct init_sec __kdp_ro;

int kdp_enable __kdp_ro = 0;
static int __check_verifiedboot __kdp_ro = 0;
static int __is_kdp_recovery __kdp_ro = 0;

#define VERITY_PARAM_LENGTH 20
static char verifiedbootstate[VERITY_PARAM_LENGTH];

#ifdef CONFIG_SAMSUNG_PRODUCT_SHIP
extern int selinux_enforcing __kdp_ro_aligned;
extern int ss_initialized __kdp_ro_aligned;
#endif

void __init kdp_init(void)
{
	struct kdp_init cred;
	memset((void *)&cred, 0, sizeof(kdp_init));
	cred._srodata = (u64)__start_rodata;
	cred._erodata = (u64)__end_rodata;
#ifdef CONFIG_RUSTUH_KDP
	cred.init_mm_pgd = (u64)swapper_pg_dir;
#endif

	cred.credSize 		= sizeof(struct cred);
	cred.sp_size		= sizeof(struct task_security_struct);
	cred.pgd_mm 		= offsetof(struct mm_struct,pgd);
	cred.uid_cred		= offsetof(struct cred,uid);
	cred.euid_cred		= offsetof(struct cred,euid);
	cred.gid_cred		= offsetof(struct cred,gid);
	cred.egid_cred		= offsetof(struct cred,egid);

	cred.bp_pgd_cred 	= offsetof(struct cred,bp_pgd);
	cred.bp_task_cred 	= offsetof(struct cred,bp_task);
	cred.type_cred 		= offsetof(struct cred,type);

	cred.security_cred 	= offsetof(struct cred,security);
	cred.usage_cred 	= offsetof(struct cred,use_cnt);
	cred.cred_task  	= offsetof(struct task_struct,cred);
	cred.mm_task 		= offsetof(struct task_struct,mm);

	cred.pid_task		= offsetof(struct task_struct,pid);
	cred.rp_task		= offsetof(struct task_struct,real_parent);
	cred.comm_task 		= offsetof(struct task_struct,comm);
	cred.bp_cred_secptr 	= offsetof(struct task_security_struct,bp_cred);
#ifndef CONFIG_RUSTUH_KDP
	cred.verifiedbootstate	= (u64)verifiedbootstate;
#endif
#ifdef CONFIG_SAMSUNG_PRODUCT_SHIP
	cred.selinux.selinux_enforcing_va  = (u64)&selinux_enforcing;
	cred.selinux.ss_initialized_va	= (u64)&ss_initialized;
#else
	cred.selinux.selinux_enforcing_va  = 0;
	cred.selinux.ss_initialized_va	= 0;
#endif
	uh_call(UH_APP_KDP, KDP_INIT, (u64)&cred, 0, 0, 0);
}

static int __init verifiedboot_state_setup(char *str)
{
	strlcpy(verifiedbootstate, str, sizeof(verifiedbootstate));

	if(!strncmp(verifiedbootstate, "orange", sizeof("orange")))
		__check_verifiedboot = 1;
	return 0;
}
__setup("androidboot.verifiedbootstate=", verifiedboot_state_setup);

static int __init boot_recovery(char *str)
{
	int temp = 0;

	if (get_option(&str, &temp)) {
		__is_kdp_recovery = temp;
		return 0;
	}
	return -EINVAL;
}
early_param("androidboot.boot_recovery", boot_recovery);

inline bool is_kdp_kmem_cache(struct kmem_cache *s)
{
	if (s->name &&
		(!strncmp(s->name, CRED_JAR_RO, strlen(CRED_JAR_RO)) ||
		 !strncmp(s->name, TSEC_JAR, strlen(TSEC_JAR)) ||
		 !strncmp(s->name, VFSMNT_JAR, strlen(VFSMNT_JAR))))
		return true;
	else
		return false;
}

#ifdef CONFIG_KDP_NS
/*------------------------------------------------
 * Namespace
 *------------------------------------------------*/
unsigned int ns_protect __kdp_ro = 0;
static int dex2oat_count = 0;
static DEFINE_SPINLOCK(mnt_vfsmnt_lock);

static struct super_block *rootfs_sb __kdp_ro = NULL;
static struct super_block *sys_sb __kdp_ro = NULL;
static struct super_block *odm_sb __kdp_ro = NULL;
static struct super_block *vendor_sb __kdp_ro = NULL;
static struct super_block *art_sb __kdp_ro = NULL;
static struct super_block *crypt_sb	__kdp_ro = NULL;
static struct super_block *dex2oat_sb	__kdp_ro = NULL;
static struct super_block *adbd_sb		__kdp_ro = NULL;
static struct kmem_cache *vfsmnt_cache __read_mostly;

void cred_ctor_vfsmount(void *data)
{
	/* Dummy constructor to make sure we have separate slabs caches. */
}
void __init kdp_mnt_init(void)
{
	struct ns_param nsparam;

	vfsmnt_cache = kmem_cache_create("vfsmnt_cache", sizeof(struct vfsmount),
				0, SLAB_HWCACHE_ALIGN | SLAB_PANIC, cred_ctor_vfsmount);

	if (!vfsmnt_cache)
		panic("Failed to allocate vfsmnt_cache \n");

	memset((void *)&nsparam, 0, sizeof(struct ns_param));
	nsparam.ns_buff_size = (u64)vfsmnt_cache->size;
	nsparam.ns_size = (u64)sizeof(struct vfsmount);
	nsparam.bp_offset = (u64)offsetof(struct vfsmount, bp_mount);
	nsparam.sb_offset = (u64)offsetof(struct vfsmount, mnt_sb);
	nsparam.flag_offset = (u64)offsetof(struct vfsmount, mnt_flags);
	nsparam.data_offset = (u64)offsetof(struct vfsmount, data);

	uh_call(UH_APP_KDP, NS_INIT, (u64)&nsparam, 0, 0, 0);
}

void __init kdp_init_mount_tree(struct vfsmount *mnt)
{
	if (!rootfs_sb)
		uh_call(UH_APP_KDP, SET_NS_SB_VFSMOUNT, (u64)&rootfs_sb, (u64)mnt, KDP_SB_ROOTFS, 0);
}

bool is_kdp_vfsmnt_cache(unsigned long addr)
{
	static void *objp;
	static struct kmem_cache *s;
	static struct page *page;

	objp = (void *)addr;

	if (!objp)
		return false;

	page = virt_to_head_page(objp);
	s = page->slab_cache;
	if (s && s == vfsmnt_cache)
		return true;
	return false;
}

static int kdp_check_sb_mismatch(struct super_block *sb)
{
	if (__is_kdp_recovery || __check_verifiedboot)
		return 0;

	if ((sb != rootfs_sb) && (sb != sys_sb) && (sb != odm_sb)
		&& (sb != vendor_sb) && (sb != art_sb) && (sb != crypt_sb)
		&& (sb != dex2oat_sb) && (sb != adbd_sb))
		return 1;

	return 0;
}

static int kdp_check_path_mismatch(struct vfsmount *vfsmnt)
{
	int i = 0;
	int ret = -1;
	char *buf = NULL;
	char *path_name = NULL;
	const char* skip_path[] = {
		"/com.android.runtime",
		"/com.android.conscrypt",
		"/com.android.art",
		"/com.android.adbd",
		"/com.android.sdkext",
	};

	if (!vfsmnt->bp_mount) {
		printk(KERN_ERR "vfsmnt->bp_mount is NULL");
		return -ENOMEM;
	}

	buf = kzalloc(PATH_MAX, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	path_name = dentry_path_raw(vfsmnt->bp_mount->mnt_mountpoint, buf, PATH_MAX);
	if (IS_ERR(path_name))
		goto out;

	for (; i < ARRAY_SIZE(skip_path); ++i) {
		if (!strncmp(path_name, skip_path[i], strlen(skip_path[i]))) {
			ret = 0;
			break;
		}
	}
out:
	kfree(buf);

	return ret;
}

int invalid_drive(struct linux_binprm * bprm)
{
	struct super_block *sb =  NULL;
	struct vfsmount *vfsmnt = NULL;

	vfsmnt = bprm->file->f_path.mnt;
	if (!vfsmnt || !is_kdp_vfsmnt_cache((unsigned long)vfsmnt)) {
		printk(KERN_ERR "[KDP] Invalid Drive : %s, vfsmnt: 0x%lx\n",
				bprm->filename, (unsigned long)vfsmnt);
		return 1;
	}

	if (!kdp_check_path_mismatch(vfsmnt)) {
		return 0;
	}

	sb = vfsmnt->mnt_sb;

	if (kdp_check_sb_mismatch(sb)) {
		printk(KERN_ERR "[KDP] Superblock Mismatch -> %s vfsmnt: 0x%lx, mnt_sb: 0x%lx",
				bprm->filename, (unsigned long)vfsmnt, (unsigned long)sb);
		printk(KERN_ERR "[KDP] Superblock list : 0x%lx, 0x%lx, 0x%lx, 0x%lx, 0x%lx, 0x%lx, 0x%lx, 0x%lx\n",
				(unsigned long)rootfs_sb, (unsigned long)sys_sb, (unsigned long)odm_sb,
				(unsigned long)vendor_sb, (unsigned long)art_sb, (unsigned long)crypt_sb,
				(unsigned long)dex2oat_sb, (unsigned long)adbd_sb);
		return 1;
	}

	return 0;
}

#define KDP_CRED_SYS_ID 1000
int is_kdp_priv_task(void)
{
	struct cred *cred = (struct cred *)current_cred();

	if (cred->uid.val <= (uid_t)KDP_CRED_SYS_ID ||
		cred->euid.val <= (uid_t)KDP_CRED_SYS_ID ||
		cred->gid.val <= (gid_t)KDP_CRED_SYS_ID ||
		cred->egid.val <= (gid_t)KDP_CRED_SYS_ID ) {
		return 1;
	}

	return 0;
}

inline void kdp_set_mnt_root_sb(struct vfsmount *mnt, struct dentry *mnt_root, struct super_block *mnt_sb)
{
	uh_call(UH_APP_KDP, SET_NS_ROOT_SB, (u64)mnt, (u64)mnt_root, (u64)mnt_sb, 0);
}

inline void kdp_assign_mnt_flags(struct vfsmount *mnt, int flags)
{
	uh_call(UH_APP_KDP, SET_NS_FLAGS, (u64)mnt, (u64)flags, 0, 0);
}

inline void kdp_clear_mnt_flags(struct vfsmount *mnt, int flags)
{
	int f = mnt->mnt_flags;
	f &= ~flags;
	kdp_assign_mnt_flags(mnt, f);
}

inline void kdp_set_mnt_flags(struct vfsmount *mnt, int flags)
{
	int f = mnt->mnt_flags;
	f |= flags;
	kdp_assign_mnt_flags(mnt, f);
}

void kdp_set_ns_data(struct vfsmount *mnt, void *data)
{
	uh_call(UH_APP_KDP, SET_NS_DATA, (u64)mnt, (u64)data, 0, 0);
}

int kdp_mnt_alloc_vfsmount(struct mount *mnt)
{
	struct vfsmount *vfsmnt = NULL;

	vfsmnt = kmem_cache_alloc(vfsmnt_cache, GFP_KERNEL);
	if (!vfsmnt)
		return 1;

	spin_lock(&mnt_vfsmnt_lock);
#ifdef CONFIG_RUSTUH_KDP
	uh_call(UH_APP_KDP, SET_NS_BP, (u64)vfsmnt, (u64)mnt, 0, 0);
#else
	uh_call(UH_APP_KDP, ALLOC_VFSMOUNT, (u64)vfsmnt, (u64)mnt, 0, 0);
#endif
	mnt->mnt = vfsmnt;
	spin_unlock(&mnt_vfsmnt_lock);

	return 0;
}

void kdp_free_vfsmount(void *objp)
{
	kmem_cache_free(vfsmnt_cache, objp);
}

static void kdp_populate_sb(char *mount_point, struct vfsmount *mnt)
{
	if (!mount_point || !mnt)
		return;

	if (!odm_sb && !strncmp(mount_point, KDP_MOUNT_PRODUCT, KDP_MOUNT_PRODUCT_LEN))
		uh_call(UH_APP_KDP, SET_NS_SB_VFSMOUNT, (u64)&odm_sb, (u64)mnt, KDP_SB_ODM, 0);
	else if (!sys_sb && !strncmp(mount_point, KDP_MOUNT_SYSTEM, KDP_MOUNT_SYSTEM_LEN))
		uh_call(UH_APP_KDP, SET_NS_SB_VFSMOUNT, (u64)&sys_sb, (u64)mnt, KDP_SB_SYS, 0);
	else if (!vendor_sb && !strncmp(mount_point, KDP_MOUNT_VENDOR, KDP_MOUNT_VENDOR_LEN))
		uh_call(UH_APP_KDP, SET_NS_SB_VFSMOUNT, (u64)&vendor_sb, (u64)mnt, KDP_SB_VENDOR, 0);
	else if (!art_sb && !strncmp(mount_point, KDP_MOUNT_ART, KDP_MOUNT_ART_LEN - 1))
		uh_call(UH_APP_KDP, SET_NS_SB_VFSMOUNT, (u64)&art_sb, (u64)mnt, KDP_SB_ART, 0);
	else if (!crypt_sb && !strncmp(mount_point, KDP_MOUNT_CRYPT, KDP_MOUNT_CRYPT_LEN - 1))
		uh_call(UH_APP_KDP, SET_NS_SB_VFSMOUNT, (u64)&crypt_sb, (u64)mnt, KDP_SB_CRYPT, 0);
	else if (!dex2oat_sb && !strncmp(mount_point, KDP_MOUNT_DEX2OAT, KDP_MOUNT_DEX2OAT_LEN - 1))
		uh_call(UH_APP_KDP, SET_NS_SB_VFSMOUNT, (u64)&dex2oat_sb, (u64)mnt, KDP_SB_DEX2OAT, 0);
	else if (!dex2oat_count && !strncmp(mount_point, KDP_MOUNT_DEX2OAT, KDP_MOUNT_DEX2OAT_LEN)) {
		uh_call(UH_APP_KDP, SET_NS_SB_VFSMOUNT, (u64)&dex2oat_sb, (u64)mnt, KDP_SB_DEX2OAT, 0);
		dex2oat_count++;
	}
	else if (!adbd_sb && !strncmp(mount_point, KDP_MOUNT_ADBD, KDP_MOUNT_ADBD_LEN - 1))
		uh_call(UH_APP_KDP, SET_NS_SB_VFSMOUNT, (u64)&adbd_sb, (u64)mnt, KDP_SB_ADBD, 0);
}

int kdp_do_new_mount(struct vfsmount *mnt, struct path *path)
{
	char *buf = NULL;
	char *dir_name;

	buf = kzalloc(PATH_MAX, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	dir_name = dentry_path_raw(path->dentry, buf, PATH_MAX);
	if (!sys_sb || !odm_sb || !vendor_sb || !art_sb || !crypt_sb || !dex2oat_sb || !dex2oat_count || !adbd_sb)
		kdp_populate_sb(dir_name, mnt);

	kfree(buf);

	return 0;
}
#endif
