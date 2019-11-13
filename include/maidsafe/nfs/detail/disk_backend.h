/*  Copyright 2015 MaidSafe.net limited

    This MaidSafe Software is licensed to you under (1) the MaidSafe.net Commercial License,
    version 1.0 or later, or (2) The General Public License (GPL), version 3, depending on which
    licence you accepted on initial access to the Software (the "Licences").

    By contributing code to the MaidSafe Software, or to this project generally, you agree to be
    bound by the terms of the MaidSafe Contributor Agreement, version 1.0, found in the root
    directory of this project at LICENSE, COPYING and CONTRIBUTOR respectively and also
    available at: http://www.maidsafe.net/licenses

    Unless required by applicable law or agreed to in writing, the MaidSafe Software distributed
    under the GPL Licence is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS
    OF ANY KIND, either express or implied.

    See the Licences for the specific language governing permissions and limitations relating to
    use of the MaidSafe Software.                                                                 */
#ifndef MAIDSAFE_NFS_DETAIL_DISK_BACKEND_H_
#define MAIDSAFE_NFS_DETAIL_DISK_BACKEND_H_

#include <cstdint>
#include <mutex>
#include <vector>

#include "boost/filesystem/path.hpp"

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4702)
#endif
#include "boost/thread/future.hpp"
#ifdef _MSC_VER
#pragma warning(pop)
#endif

#include "maidsafe/common/data_types/immutable_data.h"
#include "maidsafe/nfs/container_version.h"
#include "maidsafe/nfs/detail/container_id.h"
#include "maidsafe/nfs/detail/network.h"
#include "maidsafe/nfs/expected.h"

namespace maidsafe {
namespace nfs {
namespace detail {

// For legacy reasons, the network and disk versions are not using virtual dispatch
class DiskBackend : public Network::Interface {
 public:
  explicit DiskBackend(const boost::filesystem::path& disk_path, DiskUsage max_disk_usage);
  virtual ~DiskBackend();

 private:
  DiskBackend(const DiskBackend&) = delete;
  DiskBackend(DiskBackend&&) = delete;

  DiskBackend& operator=(const DiskBackend&) = delete;
  DiskBackend& operator=(DiskBackend&&) = delete;

  virtual boost::future<void> DoCreateSDV(
      const ContainerId& container_id,
      const ContainerVersion& initial_version,
      std::uint32_t max_versions,
      std::uint32_t max_branches) override final;
  virtual boost::future<void> DoPutSDVVersion(
      const ContainerId& container_id,
      const ContainerVersion& old_version,
      const ContainerVersion& new_version) override final;
  virtual boost::future<std::vector<ContainerVersion>> DoGetBranches(
      const ContainerId& container_id) override final;
  virtual boost::future<std::vector<ContainerVersion>> DoGetBranchVersions(
      const ContainerId& container_id, const ContainerVersion& tip) override final;

  virtual boost::future<void> DoPutChunk(const ImmutableData& data) override final;
  virtual boost::future<ImmutableData> DoGetChunk(
      const ImmutableData::NameAndTypeId& name) override final;

 private:
  bool HasDiskSpace(std::uintmax_t required_space) const;
  Expected<boost::filesystem::path> KeyToFilePath(
      const Data::NameAndTypeId& key, bool create_if_missing) const;

  Expected<std::unique_ptr<StructuredDataVersions>> ReadVersions(
      const Data::NameAndTypeId& key) const;
  Expected<void> WriteVersions(
      const Data::NameAndTypeId& key, const StructuredDataVersions& versions, const bool creation);

  Expected<void> Write(const boost::filesystem::path& path, const NonEmptyString& value);

 private:
  const boost::filesystem::path disk_path_;
  const DiskUsage max_disk_usage_;
  DiskUsage current_disk_usage_;
  std::mutex mutex_;
};

}  // namespace detail
}  // namespace nfs
}  // namespace maidsafe

#endif  // MAIDSAFE_NFS_DETAIL_DISK_BACKEND_H_
