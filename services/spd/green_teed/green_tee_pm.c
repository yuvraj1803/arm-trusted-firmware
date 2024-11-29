#include <assert.h>

#include <arch_helpers.h>
#include <common/bl_common.h>
#include <common/debug.h>
#include <lib/el3_runtime/context_mgmt.h>
#include <plat/common/platform.h>
#include <stdint.h>
#include <common/debug.h>

#include "green_teed.h"

extern green_tee_cpu_context_t cpu_context_data[GREEN_TEED_CORE_COUNT];
extern green_tee_vector_table_t vector_table;

void green_teed_cpu_on(u_register_t target_cpu){

}
int32_t green_teed_cpu_off(u_register_t __unused unused){
	int32_t rc = 0;
	uint32_t linear_id = plat_my_core_pos();
	green_tee_cpu_context_t *green_tee_ctx = &cpu_context_data[linear_id];

	if (green_tee_ctx->state == GREEN_TEE_STATE_UNKNOWN) {
		return 0;
	}

	assert(green_tee_ctx->state == GREEN_TEE_STATE_ON);

	cm_set_elr_el3(SECURE, (uint64_t) vector_table.cpu_off_entry);
	rc = green_teed_synchronous_sp_entry(green_tee_ctx);


	if (rc != 0)
		panic();

	green_tee_ctx->state = GREEN_TEE_STATE_OFF;

	 return 0;
}
void green_teed_cpu_suspend(u_register_t max_off_pwrlvl){

    int32_t rc = 0;
	uint32_t linear_id = plat_my_core_pos();
	green_tee_cpu_context_t *green_tee_ctx = &cpu_context_data[linear_id];

	if (green_tee_ctx->state == GREEN_TEE_STATE_UNKNOWN) {
		return ;
	}

	assert(green_tee_ctx->state == GREEN_TEE_STATE_ON);

	write_ctx_reg(get_gpregs_ctx(&green_tee_ctx->cpu_context), CTX_GPREG_X0,
		      max_off_pwrlvl);

	cm_set_elr_el3(SECURE, (uint64_t) vector_table.cpu_suspend_entry);
	rc = green_teed_synchronous_sp_entry(green_tee_ctx);

	if (rc != 0)
		panic();

	green_tee_ctx->state = GREEN_TEE_STATE_SUSPEND;

}
void green_teed_cpu_on_finish(u_register_t __unused unused){

    int32_t rc = 0;
	uint32_t linear_id = plat_my_core_pos();
	green_tee_cpu_context_t *green_tee_ctx = &cpu_context_data[linear_id];
	entry_point_info_t green_tee_on_entrypoint;

	assert(green_tee_ctx->state == GREEN_TEE_STATE_OFF ||
	       green_tee_ctx->state == GREEN_TEE_STATE_UNKNOWN);

	green_teed_init_ep_state(&green_tee_on_entrypoint,
				(uint64_t)vector_table.cpu_on_entry,
				0, 0, 0, 0, green_tee_ctx);

	cm_init_my_context(&green_tee_on_entrypoint);

	rc = green_teed_synchronous_sp_entry(green_tee_ctx);

	if (rc != 0)
		panic();

	green_tee_ctx->state = GREEN_TEE_STATE_ON;

}
void green_teed_cpu_suspend_finish(u_register_t max_off_pwrlvl){
	int32_t rc = 0;
	uint32_t linear_id = plat_my_core_pos();
	green_tee_cpu_context_t *green_tee_ctx = &cpu_context_data[linear_id];

	if (green_tee_ctx->state == GREEN_TEE_STATE_UNKNOWN) {
		return ;
	}

	assert(green_tee_ctx->state == GREEN_TEE_STATE_SUSPEND);

	write_ctx_reg(get_gpregs_ctx(&green_tee_ctx->cpu_context), CTX_GPREG_X0,
		      max_off_pwrlvl);
	cm_set_elr_el3(SECURE, (uint64_t) vector_table.cpu_resume_entry);
	rc = green_teed_synchronous_sp_entry(green_tee_ctx);

	if (rc != 0)
		panic();

	green_tee_ctx->state = GREEN_TEE_STATE_ON;
}
int32_t green_teed_migrate(u_register_t from_cpu, u_register_t to_cpu){
    return 0;
}
int32_t green_teed_migrate_info(u_register_t *resident_cpu){
    return 0;
}
void green_teed_system_off(void){
	uint32_t linear_id = plat_my_core_pos();
	green_tee_cpu_context_t *green_tee_ctx = &cpu_context_data[linear_id];

    if (green_tee_ctx->state == GREEN_TEE_STATE_UNKNOWN) {
		green_teed_cpu_on_finish(0);
	}

	assert(green_tee_ctx->state == GREEN_TEE_STATE_ON);

	cm_set_elr_el3(SECURE, (uint64_t) vector_table.system_off_entry);


    green_teed_synchronous_sp_entry(green_tee_ctx);
}
void green_teed_system_reset(void){
	uint32_t linear_id = plat_my_core_pos();
	green_tee_cpu_context_t *green_tee_ctx = &cpu_context_data[linear_id];


    if (green_tee_ctx->state == GREEN_TEE_STATE_UNKNOWN) {
		green_teed_cpu_on_finish(0);
	}

	assert(green_tee_ctx->state == GREEN_TEE_STATE_SUSPEND);

	cm_set_elr_el3(SECURE, (uint64_t) vector_table.system_reset_entry);

	green_teed_synchronous_sp_entry(green_tee_ctx);

}

const spd_pm_ops_t green_teed_pm = {
	.svc_on = green_teed_cpu_on,
	.svc_off = green_teed_cpu_off,
	.svc_suspend = green_teed_cpu_suspend,
	.svc_on_finish = green_teed_cpu_on_finish,
	.svc_suspend_finish = green_teed_cpu_suspend_finish,
	.svc_migrate = NULL,
	.svc_migrate_info = green_teed_migrate_info,
	.svc_system_off = green_teed_system_off,
	.svc_system_reset = green_teed_system_reset,
};
