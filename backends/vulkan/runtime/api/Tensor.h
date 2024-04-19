/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

// @lint-ignore-every CLANGTIDY facebook-hte-BadMemberName

#include <executorch/backends/vulkan/runtime/api/Context.h>
#include <executorch/backends/vulkan/runtime/api/Types.h>

namespace vkcompute {

struct LastAccess {
  api::PipelineStageFlags stage;
  api::MemoryAccessFlags access;

  LastAccess()
      : stage{api::PipelineStage::NO_STAGE},
        access{api::MemoryAccessType::NONE} {}

  LastAccess(
      api::PipelineStageFlags stage_flags,
      api::MemoryAccessFlags access_flags)
      : stage{stage_flags}, access{access_flags} {}
};

class vTensorStorage final {
 public:
  // Do not allow empty vTensorStorage construction
  vTensorStorage() = default;

  vTensorStorage(
      api::Context* context,
      const api::StorageType storage_type,
      const api::GPUMemoryLayout gpu_memory_layout,
      const std::vector<int64_t>& sizes,
      const api::ScalarType dtype,
      const bool allocate_memory = true);

  vTensorStorage(const vTensorStorage& other) = delete;
  vTensorStorage& operator=(const vTensorStorage& other) = delete;

  vTensorStorage(vTensorStorage&& other) = default;
  vTensorStorage& operator=(vTensorStorage&& other) = default;

  ~vTensorStorage();

  friend class vTensor;

 private:
  // Context
  api::Context* context_{};

  api::StorageType storage_type_;

  // Resource sizings
  api::utils::uvec3 extents_{};
  int64_t buffer_length_{};

  // Image Texture
  mutable api::VulkanImage image_;
  mutable api::VulkanBuffer buffer_;

  // Last Access - used to insert memory barriers
  LastAccess last_access_;

 private:
  // Registers underlying memory for cleanup
  void flush();

  // Memory barrier insertion
  void transition(
      api::PipelineBarrier&,
      const api::PipelineStageFlags,
      const api::MemoryAccessFlags);

  // Validation
  void verify() const;

 public:
  inline VkFormat texture_format() {
    return image_.format();
  }

  void discard_and_reallocate(
      const std::vector<int64_t>& gpu_sizes,
      const api::GPUMemoryLayout gpu_memory_layout,
      const api::ScalarType dtype);
};

class vTensor final {
 public:
  explicit vTensor(
      api::Context* context,
      const std::vector<int64_t>& sizes,
      const api::ScalarType dtype,
      const api::StorageType storage_type = api::kTexture3D,
      const api::GPUMemoryLayout memory_layout = api::kChannelsPacked,
      const bool allocate_memory = true);

  vTensor(const vTensor& other) = delete;
  vTensor& operator=(const vTensor& other) = delete;

  vTensor(vTensor&& other) = default;
  vTensor& operator=(vTensor&& other) = default;

 private:
  api::ScalarType dtype_;
  api::GPUMemoryLayout memory_layout_;

  std::vector<int64_t> sizes_;
  std::vector<int64_t> gpu_sizes_;

  // A Vulkan uniform buffer containing the tensor sizes in WHCN that can be
  // passed into a shader.
  std::shared_ptr<api::UniformParamsBuffer> cpu_sizes_uniform_;

  // A Vulkan uniform buffer containing the GPU tensor sizes in WHCN that can
  // be passed into a shader. GPU sizes refers to the sizes of the tensor after
  // padding has been applied to one dimension to align it to the next multiple
  // of 4.
  std::shared_ptr<api::UniformParamsBuffer> gpu_sizes_uniform_;

  // A Vulkan uniform buffer containing the image extents of the underlying
  // image texture that can be passed into a shader.
  std::shared_ptr<api::UniformParamsBuffer> extents_uniform_;

  vTensorStorage storage_;

 public:
  /*
   Texture Access
  */

  inline api::VulkanImage& image() const& {
    return storage_.image_;
  }

  api::VulkanImage& image(
      api::PipelineBarrier&,
      const api::PipelineStageFlags) &;

  api::VulkanImage& image(
      api::PipelineBarrier&,
      const api::PipelineStageFlags,
      const api::MemoryAccessFlags) &;

  inline api::VulkanBuffer& buffer() const& {
    return storage_.buffer_;
  }

  api::VulkanBuffer& buffer(
      api::PipelineBarrier&,
      const api::PipelineStageFlags) &;

  api::VulkanBuffer& buffer(
      api::PipelineBarrier&,
      const api::PipelineStageFlags,
      const api::MemoryAccessFlags) &;

  /*
    Metadata
  */

  inline api::StorageType storage_type() const {
    return storage_.storage_type_;
  }

  inline const api::utils::uvec3& extents() const {
    return storage_.extents_;
  }

  /*
   * Extract an `api::ScalarType` from the TensorOptions member
   */
  inline api::ScalarType dtype() const {
    return dtype_;
  }

  inline api::GPUMemoryLayout gpu_memory_layout() const {
    return memory_layout_;
  }

  inline int32_t gpu_memory_layout_int() const {
    return static_cast<int32_t>(memory_layout_);
  }

  inline const std::vector<int64_t>& sizes() const {
    return sizes_;
  }

  inline const int64_t size(size_t dim) const {
    return sizes().at(dim);
  }

  inline const int64_t dim() const {
    return sizes_.size();
  }

  /*
   * Get a uniform buffer object containing the tensor sizes to use in a compute
   * shader. Note that the UBO will be created the first time this function is
   * called.
   */
  std::shared_ptr<api::UniformParamsBuffer> cpu_sizes_ubo();

  /*
   * Get a uniform buffer object containing the tensor GPU sizes to use in a
   * compute shader. Note that the UBO will be created the first time this
   * function is called.
   */
  std::shared_ptr<api::UniformParamsBuffer> gpu_sizes_ubo();

  /*
   * Get a uniform buffer object containing the image extents to use in a
   * compute shader. Note that the UBO will be created the first time this
   * function is called.
   */
  std::shared_ptr<api::UniformParamsBuffer> extents_ubo();

  inline size_t numel() const {
    return api::utils::multiply_integers(sizes());
  }

  inline size_t nbytes() const {
    return api::element_size(dtype()) * numel();
  }

  /*
   * Returns numel but based on gpu_sizes_ instead of sizes_
   */
  inline size_t gpu_numel() const {
    return api::utils::multiply_integers(gpu_sizes_);
  }

  /*
   * Return nbytes but based on gpu_sizes_ instead of sizes_
   */
  inline VkDeviceSize gpu_nbytes() const {
    return api::element_size(dtype()) * gpu_numel();
  }

  /*
   * Return the VmaAllocationCreateInfo of the underlying resource
   */
  VmaAllocationCreateInfo get_allocation_create_info() const;

  /*
   * Return the VkMemoryRequirements of the underlying resource
   */
  VkMemoryRequirements get_memory_requirements() const;

  /*
   * Binds the underlying resource to the given memory allocation
   */
  void bind_allocation(const api::MemoryAllocation& allocation);

 private:
  /*
   * Update the size metadata of the vTensor to be new sizes. Should not be used
   * directly, reallocate() or virtual_resize() should be used instead.
   */
  void update_size_metadata(const std::vector<int64_t>& new_sizes);

 public:
  /*
   * Discard the underlying VkImage or VkBuffer and re-allocate based on new
   * tensor sizes
   */
  void reallocate(const std::vector<int64_t>& new_sizes);

  /*
   * Perform a virtual resize of the vTensor by modifying the size metadata that
   * gets used in compute shaders. This allows the shader to treat the
   * underlying resource as if it were a different size.
   */
  void virtual_resize(const std::vector<int64_t>& new_sizes);
};

} // namespace vkcompute
