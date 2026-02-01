#include <linux/spinlock.h>
#include <linux/io.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/err.h>

#include <soc/cpm.h>
#include <soc/base.h>
#include <soc/extal.h>
#include <jz_notifier.h>


#include "clk.h"

#undef DEBUG
// #define DEBUG

static DEFINE_SPINLOCK(cpm_gate_lock);

static int cpm_gate_enable(struct clk *clk,int on){
	int bit = CLK_GATE_BIT(clk->flags);
	unsigned int clkgr[2] = {CPM_CLKGR,CPM_CLKGR1};
	unsigned long flags;
	spin_lock_irqsave(&cpm_gate_lock,flags);

	/* #### TW:  dup for i2c  */
	if (!strcmp(clk->name, "i2c0")) {
		uint clk_flags;
		int bit1; 
		if ((clk->flags & 0xff000000) == 0x07000000) {
			clk_flags = (clk->flags & 0x00FFFFFF) | 0x08000000;  // replace with i2c-1 gate
		}
		else {
			clk_flags = (clk->flags & 0x00FFFFFF) | 0x07000000;  // replace with i2c-1 gate for swapped i2c bus
		}
		bit1 = CLK_GATE_BIT(clk_flags);
		if(on) {
			cpm_clear_bit(bit1 % 32, clkgr[bit1 / 32]);
		} else {
			cpm_set_bit(bit1 % 32, clkgr[bit1 / 32]);
		}
#ifdef DEBUG
		printk(" * clk gate:  add clk, flags = %x,%d,on=%d\n", clk_flags, bit1, on);
#endif
	}
	if(on) {
		cpm_clear_bit(bit % 32, clkgr[bit / 32]);
	} else {
		cpm_set_bit(bit % 32, clkgr[bit / 32]);
	}
#ifdef DEBUG
	printk("   clk gate:  name = %s, flags = %x,%d - on=%d\n", clk->name, clk->flags, bit, on);
#endif
	spin_unlock_irqrestore(&cpm_gate_lock,flags);
	return 0;
}
static struct clk_ops clk_gate_ops = {
	.enable	= cpm_gate_enable,
};

void update_gate_clk(struct clk *clk)
{
	int id = 0;
	static  unsigned long clkgr[2]={0};

	int bit = CLK_GATE_BIT(clk->flags);
	clkgr[0] = cpm_inl(CPM_CLKGR);
	clkgr[1] = cpm_inl(CPM_CLKGR1);
	if (clk->flags & CLK_FLG_PARENT) {
		id = CLK_PARENT(clk->flags);
		clk->parent = get_clk_from_id(id);
	}else
		clk->parent = get_clk_from_id(CLK_ID_EXT1);
	clk->rate = clk_get_rate(clk->parent);
	
	if(clkgr[bit / 32] & (1 << (bit % 32))) {
		clk->flags &= ~(CLK_FLG_ENABLE);
#ifdef DEBUG
		printk("clk update:  name = %s, flags = %x,%d - off\n", clk->name, clk->flags, bit);
#endif
	}else {
		clk->flags |= CLK_FLG_ENABLE;
#ifdef DEBUG
		printk("clk update:  name = %s, flags = %x,%d - on\n", clk->name, clk->flags, bit);
#endif
	}
}

void __init init_gate_clk(struct clk *clk)
{
	int id = 0;
	static  unsigned long clkgr[2]={0};
	static  int clkgr_init = 0;
	int bit = CLK_GATE_BIT(clk->flags);
	if(clkgr_init == 0){
		clkgr[0] = cpm_inl(CPM_CLKGR);
		clkgr[1] = cpm_inl(CPM_CLKGR1);
		clkgr_init = 1;
	}
	if (clk->flags & CLK_FLG_PARENT) {
		id = CLK_PARENT(clk->flags);
		clk->parent = get_clk_from_id(id);
	}else
		clk->parent = get_clk_from_id(CLK_ID_EXT1);/*TODO: check what is ext0/1*/
	clk->rate = clk_get_rate(clk->parent);
	printk("init:  name = %s, flags = %x -- %d - ", clk->name, clk->flags, bit);
	if(clkgr[bit / 32] & (1 << (bit % 32))) {
		clk->flags &= ~(CLK_FLG_ENABLE);
		// printk("clk_init:  name = %s, flags = %x,%d - off\n", clk->name, clk->flags, bit);
		cpm_gate_enable(clk,0);
	}else {
		clk->flags |= CLK_FLG_ENABLE;
		// printk("clk_init:  name = %s, flags = %x,%d - on\n", clk->name, clk->flags, bit);
		cpm_gate_enable(clk,1);
	}
	clk->ops = &clk_gate_ops;
}
