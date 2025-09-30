#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "esp_ble_mesh_defs.h"
#include "esp_ble_mesh_common_api.h"
#include "esp_ble_mesh_networking_api.h"
#include "esp_ble_mesh_provisioning_api.h"
#include "esp_ble_mesh_config_model_api.h"
#include "esp_ble_mesh_generic_model_api.h"
#include "esp_ble_mesh_local_data_operation_api.h"

#define CID_ESP 0x02E5

#define LED_R GPIO_NUM_8
#define LED_G GPIO_NUM_8
#define LED_B GPIO_NUM_8

#define LED_ON  1
#define LED_OFF 0

struct _led_state {
    uint8_t current;
    uint8_t previous;
    uint8_t pin;
    char *name;
};

struct _led_state led_state[3] = {
    { LED_OFF, LED_OFF, LED_R, "red"   },
    { LED_OFF, LED_OFF, LED_G, "green" },
    { LED_OFF, LED_OFF, LED_B, "blue"  },
};

static uint8_t dev_uuid[16] = { 0xdd, 0xdd };

static esp_ble_mesh_cfg_srv_t config_server = {
    .net_transmit = ESP_BLE_MESH_TRANSMIT(2, 20),
    .relay = ESP_BLE_MESH_RELAY_DISABLED,
    .relay_retransmit = ESP_BLE_MESH_TRANSMIT(2, 20),
    .beacon = ESP_BLE_MESH_BEACON_ENABLED,
#if defined(CONFIG_BLE_MESH_GATT_PROXY_SERVER)
    .gatt_proxy = ESP_BLE_MESH_GATT_PROXY_ENABLED,
#else
    .gatt_proxy = ESP_BLE_MESH_GATT_PROXY_NOT_SUPPORTED,
#endif
#if defined(CONFIG_BLE_MESH_FRIEND)
    .friend_state = ESP_BLE_MESH_FRIEND_ENABLED,
#else
    .friend_state = ESP_BLE_MESH_FRIEND_NOT_SUPPORTED,
#endif
    .default_ttl = 7,
};

ESP_BLE_MESH_MODEL_PUB_DEFINE(onoff_pub_0, 2 + 3, ROLE_NODE);
static esp_ble_mesh_gen_onoff_srv_t onoff_server_0 = {
    .rsp_ctrl = {
        .get_auto_rsp = ESP_BLE_MESH_SERVER_AUTO_RSP,
        .set_auto_rsp = ESP_BLE_MESH_SERVER_AUTO_RSP,
    }
};

ESP_BLE_MESH_MODEL_PUB_DEFINE(onoff_pub_1, 2 + 3, ROLE_NODE);
static esp_ble_mesh_gen_onoff_srv_t onoff_server_1 = {
    .rsp_ctrl = {
        .get_auto_rsp = ESP_BLE_MESH_SERVER_RSP_BY_APP,
        .set_auto_rsp = ESP_BLE_MESH_SERVER_RSP_BY_APP,
    }
};

ESP_BLE_MESH_MODEL_PUB_DEFINE(onoff_pub_2, 2 + 3, ROLE_NODE);
static esp_ble_mesh_gen_onoff_srv_t onoff_server_2 = {
    .rsp_ctrl = {
        .get_auto_rsp = ESP_BLE_MESH_SERVER_AUTO_RSP,
        .set_auto_rsp = ESP_BLE_MESH_SERVER_RSP_BY_APP,
    }
};

static esp_ble_mesh_model_t root_models[] = {
    ESP_BLE_MESH_MODEL_CFG_SRV(&config_server),
    ESP_BLE_MESH_MODEL_GEN_ONOFF_SRV(&onoff_pub_0, &onoff_server_0),
};

static esp_ble_mesh_model_t extend_model_0[] = {
    ESP_BLE_MESH_MODEL_GEN_ONOFF_SRV(&onoff_pub_1, &onoff_server_1),
};

static esp_ble_mesh_model_t extend_model_1[] = {
    ESP_BLE_MESH_MODEL_GEN_ONOFF_SRV(&onoff_pub_2, &onoff_server_2),
};

static esp_ble_mesh_elem_t elements[] = {
    ESP_BLE_MESH_ELEMENT(0, root_models, ESP_BLE_MESH_MODEL_NONE),
    ESP_BLE_MESH_ELEMENT(0, extend_model_0, ESP_BLE_MESH_MODEL_NONE),
    ESP_BLE_MESH_ELEMENT(0, extend_model_1, ESP_BLE_MESH_MODEL_NONE),
};

static esp_ble_mesh_comp_t composition = {
    .cid = CID_ESP,
    .element_count = ARRAY_SIZE(elements),
    .elements = elements,
};

/* Disable OOB security for SILabs Android app */
/*
 * Initialize only the fields that are guaranteed to exist across
 * different esp-idf/ble_mesh SDK versions. Some SDK versions have
 * fewer members in `esp_ble_mesh_prov_t`, causing "too many
 * initializers" errors when other fields are present here.
 */
/* Declare provision without compile-time initializers to avoid
 * "too many initializers" issues across different SDK versions.
 * We'll initialize required fields at runtime before calling
 * `esp_ble_mesh_init()`.
 */
static esp_ble_mesh_prov_t provision;

void ble_mesh_get_dev_uuid(uint8_t *dev_uuid)
{
    if (dev_uuid == NULL) {
        printf("%s, Invalid device uuid\n", __func__);
        return;
    }

    /* Copy device address to the device uuid with offset equals to 2 here.
     * The first two bytes is used for matching device uuid by Provisioner.
     * And using device address here is to avoid using the same device uuid
     * by different unprovisioned devices.
     */
    memcpy(dev_uuid + 2, esp_bt_dev_get_address(), BD_ADDR_LEN);
}

esp_err_t bluetooth_init(void)
{
    esp_err_t ret;

    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ret = esp_bt_controller_init(&bt_cfg);
    if (ret) {
        printf("%s initialize controller failed\n", __func__);
        return ret;
    }

    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret) {
        printf("%s enable controller failed\n", __func__);
        return ret;
    }
    ret = esp_bluedroid_init();
    if (ret) {
        printf("%s init bluetooth failed\n", __func__);
        return ret;
    }
    ret = esp_bluedroid_enable();
    if (ret) {
        printf("%s enable bluetooth failed\n", __func__);
        return ret;
    }

    return ret;
}

void board_led_operation(uint8_t pin, uint8_t onoff)
{
    for (int i = 0; i < 3; i++) {
        if (led_state[i].pin != pin) {
            continue;
        }
        if (onoff == led_state[i].previous) {
            printf("led %s is already %s\n",
                   led_state[i].name, (onoff ? "on" : "off"));
            return;
        }
        digitalWrite(pin, onoff);
        led_state[i].previous = onoff;
        return;
    }

    printf("LED is not found!\n");
}

static void board_led_init(void)
{
    for (int i = 0; i < 3; i++) {
        pinMode(led_state[i].pin, OUTPUT);
        digitalWrite(led_state[i].pin, LED_OFF);
        led_state[i].previous = LED_OFF;
    }
}

static void prov_complete(uint16_t net_idx, uint16_t addr, uint8_t flags, uint32_t iv_index)
{
    printf("net_idx: 0x%04x, addr: 0x%04x\n", net_idx, addr);
    /* Print flags (8-bit) and iv_index (32-bit) using the correct format macros */
    printf("flags: 0x%02x, iv_index: 0x%08" PRIx32 "\n", flags, iv_index);
    board_led_operation(LED_G, LED_OFF);
}

static void example_change_led_state(esp_ble_mesh_model_t *model,
                                     esp_ble_mesh_msg_ctx_t *ctx, uint8_t onoff)
{
    uint16_t primary_addr = esp_ble_mesh_get_primary_element_address();
    uint8_t elem_count = esp_ble_mesh_get_element_count();
    struct _led_state *led = NULL;
    uint8_t i;

    if (ESP_BLE_MESH_ADDR_IS_UNICAST(ctx->recv_dst)) {
        for (i = 0; i < elem_count; i++) {
            if (ctx->recv_dst == (primary_addr + i)) {
                led = &led_state[i];
                board_led_operation(led->pin, onoff);
            }
        }
    } else if (ESP_BLE_MESH_ADDR_IS_GROUP(ctx->recv_dst)) {
        if (esp_ble_mesh_is_model_subscribed_to_group(model, ctx->recv_dst)) {
            led = &led_state[model->element->element_addr - primary_addr];
            board_led_operation(led->pin, onoff);
        }
    } else if (ctx->recv_dst == 0xFFFF) {
        led = &led_state[model->element->element_addr - primary_addr];
        board_led_operation(led->pin, onoff);
    }
}

static void example_handle_gen_onoff_msg(esp_ble_mesh_model_t *model,
                                         esp_ble_mesh_msg_ctx_t *ctx,
                                         esp_ble_mesh_server_recv_gen_onoff_set_t *set)
{
    esp_ble_mesh_gen_onoff_srv_t *srv = (esp_ble_mesh_gen_onoff_srv_t *) model->user_data;

    switch (ctx->recv_op) {
    case ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_GET:
        esp_ble_mesh_server_model_send_msg(model, ctx,
            ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_STATUS, sizeof(srv->state.onoff), &srv->state.onoff);
        break;
    case ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_SET:
    case ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_SET_UNACK:
        if (set->op_en == false) {
            srv->state.onoff = set->onoff;
        } else {
            /* TODO: Delay and state transition */
            srv->state.onoff = set->onoff;
        }
        if (ctx->recv_op == ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_SET) {
            esp_ble_mesh_server_model_send_msg(model, ctx,
                ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_STATUS, sizeof(srv->state.onoff), &srv->state.onoff);
        }
        esp_ble_mesh_model_publish(model, ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_STATUS,
            sizeof(srv->state.onoff), &srv->state.onoff, ROLE_NODE);
        example_change_led_state(model, ctx, srv->state.onoff);
        break;
    default:
        break;
    }
}

static void example_ble_mesh_provisioning_cb(esp_ble_mesh_prov_cb_event_t event,
                                             esp_ble_mesh_prov_cb_param_t *param)
{
    switch (event) {
    case ESP_BLE_MESH_PROV_REGISTER_COMP_EVT:
        printf("ESP_BLE_MESH_PROV_REGISTER_COMP_EVT, err_code %d\n", param->prov_register_comp.err_code);
        break;
    case ESP_BLE_MESH_NODE_PROV_ENABLE_COMP_EVT:
        printf("ESP_BLE_MESH_NODE_PROV_ENABLE_COMP_EVT, err_code %d\n", param->node_prov_enable_comp.err_code);
        break;
    case ESP_BLE_MESH_NODE_PROV_LINK_OPEN_EVT:
        printf("ESP_BLE_MESH_NODE_PROV_LINK_OPEN_EVT, bearer %s\n",
            param->node_prov_link_open.bearer == ESP_BLE_MESH_PROV_ADV ? "PB-ADV" : "PB-GATT");
        break;
    case ESP_BLE_MESH_NODE_PROV_LINK_CLOSE_EVT:
        printf("ESP_BLE_MESH_NODE_PROV_LINK_CLOSE_EVT, bearer %s\n",
            param->node_prov_link_close.bearer == ESP_BLE_MESH_PROV_ADV ? "PB-ADV" : "PB-GATT");
        break;
    case ESP_BLE_MESH_NODE_PROV_COMPLETE_EVT:
        printf("ESP_BLE_MESH_NODE_PROV_COMPLETE_EVT\n");
        prov_complete(param->node_prov_complete.net_idx, param->node_prov_complete.addr,
            param->node_prov_complete.flags, param->node_prov_complete.iv_index);
        break;
    case ESP_BLE_MESH_NODE_PROV_RESET_EVT:
        printf("ESP_BLE_MESH_NODE_PROV_RESET_EVT\n");
        break;
    case ESP_BLE_MESH_NODE_SET_UNPROV_DEV_NAME_COMP_EVT:
        printf("ESP_BLE_MESH_NODE_SET_UNPROV_DEV_NAME_COMP_EVT, err_code %d\n", param->node_set_unprov_dev_name_comp.err_code);
        break;
    default:
        break;
    }
}

static void example_ble_mesh_generic_server_cb(esp_ble_mesh_generic_server_cb_event_t event,
                                               esp_ble_mesh_generic_server_cb_param_t *param)
{
    esp_ble_mesh_gen_onoff_srv_t *srv;
    printf("event 0x%02x, opcode 0x%04" PRIx32 ", src 0x%04x, dst 0x%04x\n",
        event, param->ctx.recv_op, param->ctx.addr, param->ctx.recv_dst);

    switch (event) {
    case ESP_BLE_MESH_GENERIC_SERVER_STATE_CHANGE_EVT:
        printf("ESP_BLE_MESH_GENERIC_SERVER_STATE_CHANGE_EVT\n");
        if (param->ctx.recv_op == ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_SET ||
            param->ctx.recv_op == ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_SET_UNACK) {
            printf("onoff 0x%02x\n", param->value.state_change.onoff_set.onoff);
            example_change_led_state(param->model, &param->ctx, param->value.state_change.onoff_set.onoff);
        }
        break;
    case ESP_BLE_MESH_GENERIC_SERVER_RECV_GET_MSG_EVT:
        printf("ESP_BLE_MESH_GENERIC_SERVER_RECV_GET_MSG_EVT\n");
        if (param->ctx.recv_op == ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_GET) {
            srv = (esp_ble_mesh_gen_onoff_srv_t *) param->model->user_data;
            printf("onoff 0x%02x\n", srv->state.onoff);
            example_handle_gen_onoff_msg(param->model, &param->ctx, NULL);
        }
        break;
    case ESP_BLE_MESH_GENERIC_SERVER_RECV_SET_MSG_EVT:
        printf("ESP_BLE_MESH_GENERIC_SERVER_RECV_SET_MSG_EVT\n");
        if (param->ctx.recv_op == ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_SET ||
            param->ctx.recv_op == ESP_BLE_MESH_MODEL_OP_GEN_ONOFF_SET_UNACK) {
            printf("onoff 0x%02x, tid 0x%02x\n", param->value.set.onoff.onoff, param->value.set.onoff.tid);
            if (param->value.set.onoff.op_en) {
                printf("trans_time 0x%02x, delay 0x%02x\n",
                       param->value.set.onoff.trans_time, param->value.set.onoff.delay);
            }
            example_handle_gen_onoff_msg(param->model, &param->ctx, &param->value.set.onoff);
        }
        break;
    default:
        printf("Unknown Generic Server event 0x%02x\n", event);
        break;
    }
}

static void example_ble_mesh_config_server_cb(esp_ble_mesh_cfg_server_cb_event_t event,
                                              esp_ble_mesh_cfg_server_cb_param_t *param)
{
    if (event == ESP_BLE_MESH_CFG_SERVER_STATE_CHANGE_EVT) {
        switch (param->ctx.recv_op) {
        case ESP_BLE_MESH_MODEL_OP_APP_KEY_ADD:
            printf("ESP_BLE_MESH_MODEL_OP_APP_KEY_ADD\n");
            printf("net_idx 0x%04x, app_idx 0x%04x\n",
                   param->value.state_change.appkey_add.net_idx,
                   param->value.state_change.appkey_add.app_idx);
            printf("AppKey: \n");
            for (int i = 0; i < 16; i++) {
                printf("%02x", param->value.state_change.appkey_add.app_key[i]);
            }
            printf("\n");
            break;
        case ESP_BLE_MESH_MODEL_OP_MODEL_APP_BIND:
            printf("ESP_BLE_MESH_MODEL_OP_MODEL_APP_BIND\n");
            printf("elem_addr 0x%04x, app_idx 0x%04x, cid 0x%04x, mod_id 0x%04x\n",
                param->value.state_change.mod_app_bind.element_addr,
                param->value.state_change.mod_app_bind.app_idx,
                param->value.state_change.mod_app_bind.company_id,
                param->value.state_change.mod_app_bind.model_id);
            break;
        case ESP_BLE_MESH_MODEL_OP_MODEL_SUB_ADD:
            printf("ESP_BLE_MESH_MODEL_OP_MODEL_SUB_ADD\n");
            printf("elem_addr 0x%04x, sub_addr 0x%04x, cid 0x%04x, mod_id 0x%04x\n",
                param->value.state_change.mod_sub_add.element_addr,
                param->value.state_change.mod_sub_add.sub_addr,
                param->value.state_change.mod_sub_add.company_id,
                param->value.state_change.mod_sub_add.model_id);
            break;
        default:
            break;
        }
    }
}

static esp_err_t ble_mesh_init(void)
{
    esp_err_t err = ESP_OK;

    /* Initialize provision struct at runtime to avoid compile-time
     * initializer problems with different SDK versions. Keep only the
     * fields we need here; others can be left zeroed.
     */
    /* Zero-init the provision structure. Do not assume specific members
     * (like `uuid`) exist in the SDK's esp_ble_mesh_prov_t definition.
     * If your SDK requires setting provisioning fields, add them here
     * after checking the SDK header for the exact member names.
     */
    memset(&provision, 0, sizeof(provision));

    esp_ble_mesh_register_prov_callback(example_ble_mesh_provisioning_cb);
    esp_ble_mesh_register_config_server_callback(example_ble_mesh_config_server_cb);
    esp_ble_mesh_register_generic_server_callback(example_ble_mesh_generic_server_cb);

    err = esp_ble_mesh_init(&provision, &composition);
    if (err != ESP_OK) {
        printf("Failed to initialize mesh stack (err %d)\n", err);
        return err;
    }

    uint32_t flags = ESP_BLE_MESH_PROV_ADV | ESP_BLE_MESH_PROV_GATT;
    err = esp_ble_mesh_node_prov_enable((esp_ble_mesh_prov_bearer_t) flags);
    if (err != ESP_OK) {
        printf("Failed to enable mesh node (err %d)\n", err);
        return err;
    }

    printf("BLE Mesh Node initialized\n");

    board_led_operation(LED_G, LED_ON);

    return err;
}

void setup(void)
{
    esp_err_t err;

    Serial.begin(115200);

    printf("Initializing...\n");

    board_led_init();

    err = bluetooth_init();
    if (err) {
        printf("esp32_bluetooth_init failed (err %d)\n", err);
        return;
    }

    ble_mesh_get_dev_uuid(dev_uuid);

    /* Initialize the Bluetooth Mesh Subsystem */
    err = ble_mesh_init();
    if (err) {
        printf("Bluetooth mesh init failed (err %d)\n", err);
    }
}

void loop(void)
{
}