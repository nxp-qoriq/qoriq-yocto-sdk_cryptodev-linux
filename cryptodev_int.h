/* cipher stuff
 * Copyright 2012 Freescale Semiconductor, Inc.
 */
#ifndef CRYPTODEV_INT_H
# define CRYPTODEV_INT_H

#include <linux/version.h>

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 13, 0))
#  define reinit_completion(x) INIT_COMPLETION(*(x))
#endif

#include <linux/init.h>
#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/fdtable.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/scatterlist.h>
#include <crypto/cryptodev.h>
#include <crypto/aead.h>

#define PFX "cryptodev: "
#define dprintk(level, severity, format, a...)			\
	do {							\
		if (level <= cryptodev_verbosity)		\
			printk(severity PFX "%s[%u] (%s:%u): " format "\n",	\
			       current->comm, current->pid,	\
			       __func__, __LINE__,		\
			       ##a);				\
	} while (0)
#define derr(level, format, a...) dprintk(level, KERN_ERR, format, ##a)
#define dwarning(level, format, a...) dprintk(level, KERN_WARNING, format, ##a)
#define dinfo(level, format, a...) dprintk(level, KERN_INFO, format, ##a)
#define ddebug(level, format, a...) dprintk(level, KERN_DEBUG, format, ##a)


extern int cryptodev_verbosity;

struct fcrypt {
	struct list_head list;
	struct mutex sem;
};

/* compatibility stuff */
#ifdef CONFIG_COMPAT
#include <linux/compat.h>

/* input of CIOCGSESSION */
struct compat_session_op {
	/* Specify either cipher or mac
	 */
	uint32_t	cipher;		/* cryptodev_crypto_op_t */
	uint32_t	mac;		/* cryptodev_crypto_op_t */

	uint32_t	keylen;
	compat_uptr_t	key;		/* pointer to key data */
	uint32_t	mackeylen;
	compat_uptr_t	mackey;		/* pointer to mac key data */

	uint32_t	ses;		/* session identifier */
};

/* input of CIOCCRYPT */
struct compat_crypt_op {
	uint32_t	ses;		/* session identifier */
	uint16_t	op;		/* COP_ENCRYPT or COP_DECRYPT */
	uint16_t	flags;		/* see COP_FLAG_* */
	uint32_t	len;		/* length of source data */
	compat_uptr_t	src;		/* source data */
	compat_uptr_t	dst;		/* pointer to output data */
	compat_uptr_t	mac;/* pointer to output data for hash/MAC operations */
	compat_uptr_t	iv;/* initialization vector for encryption operations */
};

/* input of CIOCKEY */
struct compat_crparam {
	compat_uptr_t	crp_p;
	uint32_t	crp_nbits;
};

struct compat_crypt_kop {
	uint32_t	crk_op;		/* cryptodev_crk_ot_t */
	uint32_t	crk_status;
	uint16_t	crk_iparams;
	uint16_t	crk_oparams;
	uint32_t	crk_pad1;
	struct compat_crparam	crk_param[CRK_MAXPARAM];
	enum ec_curve_t curve_type; /* 0 == Discrete Log, 1 = EC_PRIME,
				 2 = EC_BINARY */
	compat_uptr_t cookie;
};

struct compat_pkc_cookie_list_s {
	int cookie_available;
	compat_uptr_t cookie[MAX_COOKIES];
	int status[MAX_COOKIES];
};

 /* input of CIOCAUTHCRYPT */
struct compat_crypt_auth_op {
	uint32_t	ses;		/* session identifier */
	uint16_t	op;		/* COP_ENCRYPT or COP_DECRYPT */
	uint16_t	flags;		/* see COP_FLAG_AEAD_* */
	uint32_t	len;		/* length of source data */
	uint32_t	auth_len;	/* length of auth data */
	compat_uptr_t	auth_src;	/* authenticated-only data */

	/* The current implementation is more efficient if data are
	 * encrypted in-place (src==dst). */
	compat_uptr_t	src;		/* data to be encrypted and
	authenticated */
	compat_uptr_t	dst;		/* pointer to output data. Must have
					 * space for tag. For TLS this should be
					 * at least len + tag_size + block_size
					 * for padding */

	compat_uptr_t	tag;		/* where the tag will be copied to. TLS
					 * mode doesn't use that as tag is
					 * copied to dst.
					 * SRTP mode copies tag there. */
	uint32_t	tag_len;	/* the length of the tag. Use zero for
					 * digest size or max tag. */

	/* initialization vector for encryption operations */
	compat_uptr_t	iv;
	uint32_t	iv_len;
};

struct compat_hash_op_data {
	compat_uptr_t	ses;
	uint32_t	mac_op;		/* cryptodev_crypto_op_t */
	compat_uptr_t	mackey;
	uint32_t	mackeylen;

	uint16_t	flags;		/* see COP_FLAG_* */
	uint32_t	len;		/* length of source data */
	compat_uptr_t	src;		/* source data */
	compat_uptr_t	mac_result;
};

/* compat ioctls, defined for the above structs */
#define COMPAT_CIOCGSESSION    _IOWR('c', 102, struct compat_session_op)
#define COMPAT_CIOCCRYPT       _IOWR('c', 104, struct compat_crypt_op)
#define COMPAT_CIOCKEY    _IOWR('c', 105, struct compat_crypt_kop)
#define COMPAT_CIOCASYNCCRYPT  _IOW('c', 107, struct compat_crypt_op)
#define COMPAT_CIOCASYNCFETCH  _IOR('c', 108, struct compat_crypt_op)
#define COMPAT_CIOCAUTHCRYPT   _IOWR('c', 109, struct compat_crypt_auth_op)
#define COMPAT_CIOCASYMASYNCRYPT    _IOW('c', 110, struct compat_crypt_kop)
#define COMPAT_CIOCASYMFETCHCOOKIE    _IOR('c', 111, \
				struct compat_pkc_cookie_list_s)
#define COMPAT_CIOCHASH	_IOWR('c', 114, struct compat_hash_op_data)
#endif /* CONFIG_COMPAT */

/* kernel-internal extension to struct crypt_kop */
struct kernel_crypt_kop {
	struct crypt_kop kop;

	struct task_struct *task;
	struct mm_struct *mm;
};

/* kernel-internal extension to struct crypt_op */
struct kernel_crypt_op {
	struct crypt_op cop;

	int ivlen;
	__u8 iv[EALG_MAX_BLOCK_LEN];

	int digestsize;
	uint8_t hash_output[AALG_MAX_RESULT_LEN];

	struct task_struct *task;
	struct mm_struct *mm;
};

struct kernel_hash_op {
	struct hash_op_data hash_op;

	int digestsize;
	uint8_t hash_output[AALG_MAX_RESULT_LEN];
	struct task_struct *task;
	struct mm_struct *mm;
};

struct kernel_crypt_auth_op {
	struct crypt_auth_op caop;

	int dst_len; /* based on src_len + pad + tag */
	int ivlen;
	__u8 iv[EALG_MAX_BLOCK_LEN];

	struct task_struct *task;
	struct mm_struct *mm;
};

/* auth */

#ifdef CONFIG_COMPAT
int compat_kcaop_from_user(struct kernel_crypt_auth_op *kcaop,
				struct fcrypt *fcr, void __user *arg);

int compat_kcaop_to_user(struct kernel_crypt_auth_op *kcaop,
				struct fcrypt *fcr, void __user *arg);
#endif /* CONFIG_COMPAT */


int kcaop_from_user(struct kernel_crypt_auth_op *kcop,
			struct fcrypt *fcr, void __user *arg);
int kcaop_to_user(struct kernel_crypt_auth_op *kcaop,
		struct fcrypt *fcr, void __user *arg);
int crypto_auth_run(struct fcrypt *fcr, struct kernel_crypt_auth_op *kcaop);
int crypto_run(struct fcrypt *fcr, struct kernel_crypt_op *kcop);
int hash_run(struct kernel_hash_op *khop);

#include <cryptlib.h>

/* Cryptodev Key operation handler */
int crypto_bn_modexp(struct cryptodev_pkc *);
int crypto_modexp_crt(struct cryptodev_pkc *);
int crypto_kop_dsasign(struct cryptodev_pkc *);
int crypto_kop_dsaverify(struct cryptodev_pkc *);
int crypto_run_asym(struct cryptodev_pkc *);
void cryptodev_complete_asym(struct crypto_async_request *, int);

/* other internal structs */
struct csession {
	struct list_head entry;
	struct mutex sem;
	struct cipher_data cdata;
	struct hash_data hdata;
	uint32_t sid;
	uint32_t alignmask;

	unsigned int array_size;
	unsigned int used_pages; /* the number of pages that are used */
	/* the number of pages marked as NOT-writable; they preceed writeables */
	unsigned int readonly_pages;
	struct page **pages;
	struct scatterlist *sg;
};

struct csession *crypto_get_session_by_sid(struct fcrypt *fcr, uint32_t sid);

static inline void crypto_put_session(struct csession *ses_ptr)
{
	mutex_unlock(&ses_ptr->sem);
}
int adjust_sg_array(struct csession *ses, int pagecount);

#endif /* CRYPTODEV_INT_H */
