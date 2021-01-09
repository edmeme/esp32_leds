#include "storage.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <nvs.h>

static const char * const TAG = "Storage";

static const int MAGIC = 0x0FA55AF0;
static const int VERSION = 1;
static const char * const NAMESPACE = "storage";
static const char * const FILENAME = "pvs";
static const int STORE_SECONDS = 10;

static unsigned int g_saved_checksum = 0; // 0 is an invalid checksum :)

typedef struct {
  unsigned int magic;
  unsigned char version;
  unsigned char size;
  persistent_state_t data;
  unsigned int checksum;
} storage_t;

static storage_t storage;

// Compute a checksum for storage
unsigned int checksum_storage(storage_t data){
  data.checksum = 0;
  unsigned int checksum = 0;
  for(int i = 0;  i < sizeof(data) / sizeof(unsigned int); ++i){
    unsigned int part =  ((unsigned int *)(&data))[i];
    unsigned int sum = checksum + part;
    checksum = sum < checksum ? ~sum : sum;
  }
  if(checksum == 0) return ~checksum; // make 0 an invalid checksum
  return checksum;
}

// Create an initial value for the internal storage, used for first-run and corrupted data.
void reinitialize_storage(storage_t * out, default_initializer_fn di){
  out->magic = MAGIC;
  out->version = VERSION;
  out->size = sizeof(storage_t);
  di(&out->data);
  out->checksum = checksum_storage(*out);
}

// Validate a storage struct
bool validate_storage(const storage_t * storage){
  bool good = true;
  if(storage->magic != MAGIC){
    ESP_LOGE(TAG, "Bad magic (got %08x expected %08x)",storage->magic, MAGIC);
    good = false;
  }
  if(storage->version != VERSION){
    ESP_LOGE(TAG, "Bad version (got %d expected %d)",storage->version, VERSION);
    good = false;
  }
  if(sizeof(storage_t) != storage->size){
    ESP_LOGE(TAG, "Bad size (got %d expected %d)",storage->size, sizeof(storage_t));
    good = false;
  }
  unsigned int ck = checksum_storage(*storage);
  if(storage->checksum != ck){
    ESP_LOGE(TAG, "Bad checksum (got %08x expected %08x)",
	     storage->checksum, ck);
    good = false;
  }
  return good;
}

// Put storage data (data) into persistent memory,
// only if the data changed (it just checks the checksum, I hope collisions are rare enough)
esp_err_t save_storage(storage_t * data){
  nvs_handle_t handle;
  esp_err_t err = ESP_OK;

  unsigned int checksum = checksum_storage(*data);
  data->checksum = checksum;
  if (g_saved_checksum == checksum){
    ESP_LOGI(TAG, "Persistent data save ignored (no changes) (sum %x08).", checksum);
    return ESP_OK;
  }

  err = nvs_open(NAMESPACE, NVS_READWRITE, &handle);
  if (err != ESP_OK) return err;
  err = nvs_set_blob(handle, FILENAME, data, sizeof(storage_t));
  if (err != ESP_OK) goto end;
  err = nvs_commit(handle);
  if (err != ESP_OK) goto end;

 end:
  if(err == ESP_OK){
    g_saved_checksum = checksum;
    ESP_LOGI(TAG, "Persistent data saved successfully (sum %x08).", checksum);
  }
  nvs_close(handle);
  return err;
}

// Read data from storage into 'out'
esp_err_t load_storage(storage_t * out, default_initializer_fn di){
  nvs_handle_t handle;
  esp_err_t err = ESP_OK;
  
  // Open
  err = nvs_open(NAMESPACE, NVS_READWRITE, &handle);
  if (err != ESP_OK) return err;
    
  size_t required_size = 0;
  err = nvs_get_blob(handle, FILENAME, NULL, &required_size); // Read the size of the existing file
  if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) goto end;
  if (required_size != sizeof(storage_t)) {
    if(required_size == 0)
      ESP_LOGI(TAG, "No persistent data, loading default values.");
    else
      ESP_LOGE(TAG, "Persistent data has wrong size, using default values.");
    reinitialize_storage(out, di);
    return ESP_OK;
  } else {
    required_size = sizeof(storage_t);
    err = nvs_get_blob(handle, FILENAME, out, &required_size);
    if (err != ESP_OK) goto end;
    ESP_LOGI(TAG, "Loaded %d bytes of persistent storage.", sizeof(storage_t));
    g_saved_checksum = out->checksum;
  }
  
  // Close
 end:
  nvs_close(handle);
  return err;
}

// Periodically called to store persistent data
static void timer_callback(void* arg){
  ESP_LOGI(TAG, "Enter scheduled event: saving persistent data if changed.");
  ESP_ERROR_CHECK(save_storage(&storage));
}

// Schedule storing color to persistent storage after some time.
// We don't immediately store data to reduce the number of write cycles that wear out the flash
// Data is only written when it changes.
void setup_save_timer()
{
  const esp_timer_create_args_t periodic_timer_args = {
    .callback = &timer_callback,
    .name = "timer-storage-save"
  };

  esp_timer_handle_t periodic_timer;
  ESP_ERROR_CHECK(esp_timer_create(&periodic_timer_args, &periodic_timer));
  ESP_ERROR_CHECK(esp_timer_start_periodic(periodic_timer, STORE_SECONDS * 1000000));
  ESP_LOGI(TAG, "Started store timer.");
}

// initialize storage
persistent_state_t * storage_initialize(default_initializer_fn di)
{
  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    // NVS partition was truncated and needs to be erased
    ESP_ERROR_CHECK(nvs_flash_erase());
    err = nvs_flash_init(); // Retry
  }
  ESP_ERROR_CHECK(err);
  
  ESP_ERROR_CHECK(load_storage(&storage, di));
  if(!validate_storage(&storage)){
    ESP_LOGI(TAG, "Storage was invalid, using defaults");
    reinitialize_storage(&storage, di);
  }
  
  setup_save_timer();
  return &storage.data;
}
