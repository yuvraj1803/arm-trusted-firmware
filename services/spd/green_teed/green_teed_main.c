
#include <common/runtime_svc.h>
#include <common/debug.h>
#include <bl31/bl31.h>
#include <bl31/interrupt_mgmt.h>
#include <plat/common/platform.h>
#include <arch_helpers.h>
#include <arch.h>
#include <lib/el3_runtime/context_mgmt.h>
#include <context.h>
#include <lib/smccc.h>
#include <lib/utils.h>

#include <stdint.h>

#include "green_teed.h"
#include "green_tee_smc.h"

green_tee_cpu_context_t cpu_context_data[GREEN_TEED_CORE_COUNT];
green_tee_vector_table_t vector_table;


void green_teed_init_ep_state(struct entry_point_info *green_tee_entry_point,
		        uint64_t pc, uint64_t arg0,
				uint64_t arg1, uint64_t arg2, uint64_t arg3,
				green_tee_cpu_context_t *green_tee_ctx)
{
	uint32_t ep_attr;

	assert(green_tee_ctx);
	assert(green_tee_entry_point);
	assert(pc);

	green_tee_ctx->mpidr_el1 = read_mpidr_el1();
	green_tee_ctx->state = GREEN_TEE_STATE_OFF;

	cm_set_context(&green_tee_ctx->cpu_context, SECURE);

	ep_attr = SECURE | EP_ST_ENABLE;

	if (read_sctlr_el3() & SCTLR_EE_BIT)
		ep_attr |= EP_EE_BIG;

	SET_PARAM_HEAD(green_tee_entry_point, PARAM_EP, VERSION_1, ep_attr);
	green_tee_entry_point->pc = pc;

	green_tee_entry_point->spsr = SPSR_64(MODE_EL1, MODE_SP_ELX, DISABLE_ALL_EXCEPTIONS);

	zeromem(&green_tee_entry_point->args, sizeof(green_tee_entry_point->args));
	green_tee_entry_point->args.arg0 = arg0;
	green_tee_entry_point->args.arg1 = arg1;
	green_tee_entry_point->args.arg2 = arg2;
	green_tee_entry_point->args.arg3 = arg3;
}


int32_t green_teed_synchronous_sp_entry(green_tee_cpu_context_t* context){
	cm_el1_sysregs_context_restore(SECURE);
	cm_set_next_eret_context(SECURE);

	uint64_t ret = green_teed_enter_sp(&context->stack);

	return (int32_t) ret;

}

int32_t green_teed_synchronous_sp_exit(green_tee_cpu_context_t* context){

	cm_el1_sysregs_context_save(SECURE);

	uint64_t ret = green_teed_exit_sp(context->stack);
	return (int32_t) ret;
}

int32_t green_teed_init(void){

	int linear_id = plat_my_core_pos();
	green_tee_cpu_context_t* my_context = &cpu_context_data[linear_id];

	if(!my_context) goto green_teed_init_fail;

	entry_point_info_t* ep_info = bl31_plat_get_next_image_ep_info(SECURE);
	if(!ep_info) goto green_teed_init_fail;

	cm_init_my_context(ep_info);

	int ret = green_teed_synchronous_sp_entry(my_context);

	return ret;

green_teed_init_fail:
	return 1;

}

int32_t green_teed_setup(void){

	int linear_id = plat_my_core_pos();
	green_tee_cpu_context_t* my_context = &cpu_context_data[linear_id];

	entry_point_info_t* ep_info = bl31_plat_get_next_image_ep_info(SECURE);
	if(!ep_info){
		WARN("Green TEE Entry Point Information Unavailable. SMC will always return SMC_UKN\n");
		goto setup_fail;
	}

	if(!ep_info->pc){
		WARN("Green TEE Entry Point Invalid\n");
		goto setup_fail;
	}

	if(read_sctlr_el3() & SCTLR_EE_BIT){ // not supporting big endian images for now.
		goto setup_fail;
	}

	green_teed_init_ep_state(ep_info, ep_info->pc,
			0, 0, 0, 0,
			my_context);

	bl31_register_bl32_init(&green_teed_init);

	return 0;

setup_fail:
	return 1;
}

void green_tee_init_vector_table(uint64_t vectors){

	green_tee_vector_table_t* table = (green_tee_vector_table_t*) vectors;

	vector_table.cpu_off_entry = table->cpu_off_entry;
	vector_table.cpu_on_entry = table->cpu_on_entry;
	vector_table.cpu_resume_entry = table->cpu_resume_entry;
	vector_table.cpu_suspend_entry = table->cpu_suspend_entry;
	vector_table.fast_smc_entry = table->fast_smc_entry;
	vector_table.fiq_entry = table->fiq_entry;
	vector_table.system_off_entry = table->system_off_entry;
	vector_table.system_reset_entry = table->system_reset_entry;
	vector_table.yield_smc_entry = table->yield_smc_entry;
	
}

static uint64_t green_teed_sel1_interrupt_handler(uint32_t id, uint32_t flags, void* handle, void* cookie){
	return 0;
}

static int green_teed_register_interrupt_handler(){
	u_register_t flags = 0;
	int ret = 0;

	set_interrupt_rm_flag(flags, NON_SECURE);
	ret = register_interrupt_type_handler(INTR_TYPE_S_EL1, green_teed_sel1_interrupt_handler, flags);

	return ret;
}

uintptr_t green_teed_smc_handler(uint32_t smc_fid, u_register_t x1, u_register_t x2, u_register_t x3, u_register_t x4, void* cookie, void* handle, u_register_t flags){

	// x1: address of exception vector table passed by S-EL1
	// x2: address of SMC handler inside TEE
	int linear_id = plat_my_core_pos();
	green_tee_cpu_context_t* cpu_context = &cpu_context_data[linear_id];

	if(!cpu_context) {
		panic();
	}

	if(is_caller_non_secure(flags)){


		switch(GET_SMC_NUM(smc_fid)){
			case GREEN_TEE_SMC_LINUX_INIT:
				INFO("Green TEED contact from non-secure world established.\n");
				break;
			case GREEN_TEE_SMC_LINUX_PRINT:
			case GREEN_TEE_SMC_LINUX_ENCRYPT:
			case GREEN_TEE_SMC_LINUX_DECRYPT:

				cm_el1_sysregs_context_save(NON_SECURE);
				cm_set_elr_el3(SECURE, vector_table.yield_smc_entry);

				cm_el1_sysregs_context_restore(SECURE);
				cm_set_next_eret_context(SECURE);

				SMC_RET4(&cpu_context->cpu_context, smc_fid, x1, x2, x3);
				
				break;
			default:
				panic();
		}
	}

	if(is_caller_secure(flags)){
		switch(GET_SMC_NUM(smc_fid)){

			case GREEN_TEE_SMC_ENTRY_DONE:

				if(x1 == 0) panic();	// S-EL1 has to return its VBAR_EL1. Otherwise, something has gone wrong...

				green_tee_init_vector_table(x1);

				cpu_context->state = GREEN_TEE_STATE_ON;

				int ret = green_teed_register_interrupt_handler();
				if(ret < 0) panic();

				psci_register_spd_pm_hook(&green_teed_pm);

				green_teed_synchronous_sp_exit(cpu_context);

				break;
			
			case GREEN_TEE_SMC_PM_ACK:
				
				green_teed_synchronous_sp_exit(cpu_context);

				break;

			case GREEN_TEE_SMC_HANDLED:

				cm_el1_sysregs_context_save(SECURE);
				cm_el1_sysregs_context_restore(NON_SECURE);
				cm_set_next_eret_context(NON_SECURE);

				SMC_RET4(cm_get_context(NON_SECURE), 0, 0, 0, 0);

				break;

			case GREEN_TEE_SMC_FAILED:
				cm_el1_sysregs_context_save(SECURE);
				cm_el1_sysregs_context_restore(NON_SECURE);
				cm_set_next_eret_context(NON_SECURE);

				SMC_RET4(cm_get_context(NON_SECURE), 1, 1, 1, 1);
				break;

			default:
				panic();
		}
	}


	return 0;

}

DECLARE_RT_SVC(green_teed_rt_svc_fast,
		OEN_TOS_START,
		OEN_TOS_END,
		SMC_TYPE_FAST,
		green_teed_setup,
		green_teed_smc_handler);

DECLARE_RT_SVC(green_teed_rt_svc_yld,
		OEN_TOS_START,
		OEN_TOS_END,
		SMC_TYPE_YIELD,
		NULL,
		green_teed_smc_handler);