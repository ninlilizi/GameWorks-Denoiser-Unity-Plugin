/*
Copyright (c) 2022, NVIDIA CORPORATION. All rights reserved.

NVIDIA CORPORATION and its licensors retain all intellectual property
and proprietary rights in and to this software, related documentation
and any modifications thereto. Any use, reproduction, disclosure or
distribution of this software and related documentation without an express
license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#pragma once

namespace nri
{
    struct DeviceVK;
    struct MemoryVK;

    struct BufferVK
    {
        BufferVK(DeviceVK& device);
        ~BufferVK();

        DeviceVK& GetDevice() const;
        VkBuffer GetHandle(uint32_t physicalDeviceIndex) const;
        VkDeviceAddress GetDeviceAddress(uint32_t physicalDeviceIndex) const;
        uint64_t GetSize() const;
        Result Create(const BufferDesc& bufferDesc);
        Result Create(const BufferVulkanDesc& bufferDesc);
        void SetHostMemory(MemoryVK& memory, uint64_t memoryOffset);
        void ReadDeviceAddress();

        void SetDebugName(const char* name);
        void GetMemoryInfo(MemoryLocation memoryLocation, MemoryDesc& memoryDesc) const;
        void* Map(uint64_t offset, uint64_t size);
        void Unmap();

    private:
        std::array<VkBuffer, PHYSICAL_DEVICE_GROUP_MAX_SIZE> m_Handles = {};
        std::array<VkDeviceAddress, PHYSICAL_DEVICE_GROUP_MAX_SIZE> m_DeviceAddresses = {};
        DeviceVK& m_Device;
        uint64_t m_Size = 0;
        MemoryVK* m_Memory = nullptr;
        uint64_t m_MappedMemoryOffset = 0;
        uint64_t m_MappedRangeOffset = 0;
        uint64_t m_MappedRangeSize = 0;
        bool m_OwnsNativeObjects = false;
    };

    inline BufferVK::BufferVK(DeviceVK& device) :
        m_Device(device)
    {
    }

    inline VkBuffer BufferVK::GetHandle(uint32_t physicalDeviceIndex) const
    {
        return m_Handles[physicalDeviceIndex];
    }

    inline VkDeviceAddress BufferVK::GetDeviceAddress(uint32_t physicalDeviceIndex) const
    {
        return m_DeviceAddresses[physicalDeviceIndex];
    }

    inline DeviceVK& BufferVK::GetDevice() const
    {
        return m_Device;
    }

    inline uint64_t BufferVK::GetSize() const
    {
        return m_Size;
    }

    inline VkDeviceAddress GetBufferDeviceAddress(const Buffer* buffer, uint32_t physicalDeviceIndex)
    {
        const BufferVK* bufferVK = (const BufferVK*)buffer;

        return bufferVK != nullptr ? bufferVK->GetDeviceAddress(physicalDeviceIndex) : 0;
    }
}