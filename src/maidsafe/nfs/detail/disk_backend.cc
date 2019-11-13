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
#include "maidsafe/nfs/detail/disk_backend.h"

#include "boost/expected/conversions/expected_to_future.hpp"
#include "boost/filesystem/operations.hpp"

#include "maidsafe/common/make_unique.h"
#include "maidsafe/common/utils.h"

namespace maidsafe {
namespace nfs {
namespace detail {
namespace {
const unsigned kDepth = 5;

DiskUsage InitialiseDiskRoot(const boost::filesystem::path& disk_root) {
  if (!boost::filesystem::exists(disk_root)) {
    boost::system::error_code error{};
    boost::filesystem::create_directories(disk_root, error);
    if (error) {
      LOG(kError) << "Can't create disk root at " << disk_root << ": " << error.message();
      BOOST_THROW_EXCEPTION(MakeError(CommonErrors::uninitialised));
    }
  }
  return DiskUsage{0};
}

/* These are temporary, it will be removed shortly when type-erased callbacks
   are provided instead in an interface update. */
template<typename Result, typename Expect, typename Error>
boost::future<Result> ConvertError(const boost::expected<Expect, Error>& error) {
  return boost::make_exceptional_future<Result>(std::system_error(error.error()));
}

template<typename Expect>
boost::future<Expect> ConvertToFuture(const Expected<Expect>& value) {
  if (value) {
    return boost::make_ready_future(*value);
  }
  return ConvertError<Expect>(value);
}

boost::future<void> ConvertToFuture(const Expected<void>& value) {
  if (value) {
    return boost::make_ready_future();
  }
  return ConvertError<void>(value);
}
}  // namespace

DiskBackend::DiskBackend(const boost::filesystem::path& disk_path, DiskUsage max_disk_usage)
  : disk_path_(disk_path),
    max_disk_usage_(max_disk_usage),
    current_disk_usage_(InitialiseDiskRoot(disk_path_)),
    mutex_() {
}

DiskBackend::~DiskBackend() {}

boost::future<void> DiskBackend::DoCreateSDV(
    const ContainerId& container_id,
    const ContainerVersion& initial_version,
    std::uint32_t max_versions,
    std::uint32_t max_branches) {
  StructuredDataVersions versions{max_versions, max_branches};
  versions.Put(StructuredDataVersions::VersionName{}, initial_version);

  const std::lock_guard<std::mutex> lock{mutex_};
  return ConvertToFuture(WriteVersions(container_id.data, versions, true));
}

boost::future<void> DiskBackend::DoPutSDVVersion(
    const ContainerId& container_id,
    const ContainerVersion& old_version,
    const ContainerVersion& new_version) {
  const std::lock_guard<std::mutex> lock{mutex_};
  auto versions = ReadVersions(container_id.data);
  if (!versions) {
    return ConvertError<void>(versions);
  }

  try {
    (*versions)->Put(old_version, new_version);
  } catch (const std::exception&) {
    return boost::make_exceptional_future<void>(boost::current_exception());
  }

  return ConvertToFuture(WriteVersions(container_id.data, **versions, false));
}

boost::future<std::vector<ContainerVersion>> DiskBackend::DoGetBranches(
    const ContainerId& container_id) {
  Expected<std::unique_ptr<StructuredDataVersions>> versions{};
  {
    const std::lock_guard<std::mutex> lock{mutex_};
    versions = ReadVersions(container_id.data);
    if (!versions) {
      return ConvertError<std::vector<ContainerVersion>>(versions);
    }
  }

  try {
    return boost::make_ready_future((*versions)->Get());
  } catch (const std::exception&) {
    return boost::make_exceptional_future<std::vector<ContainerVersion>>(
        boost::current_exception());
  }
}

boost::future<std::vector<ContainerVersion>> DiskBackend::DoGetBranchVersions(
    const ContainerId& container_id, const ContainerVersion& tip) {
  Expected<std::unique_ptr<StructuredDataVersions>> versions{};
  {
    const std::lock_guard<std::mutex> lock{mutex_};
    versions = ReadVersions(container_id.data);
    if (!versions) {
      return ConvertError<std::vector<ContainerVersion>>(versions);
    }
  }

  try {
    return boost::make_ready_future((*versions)->GetBranch(tip));
  } catch (const std::exception&) {
    return boost::make_exceptional_future<std::vector<ContainerVersion>>(
        boost::current_exception());
  }
}

boost::future<void> DiskBackend::DoPutChunk(const ImmutableData& data) {
  {
    const std::lock_guard<std::mutex> lock{mutex_};
    if (!boost::filesystem::exists(disk_path_)) {
      return boost::make_exceptional_future<void>(MakeError(CommonErrors::filesystem_io_error));
    }

    const auto file_path = KeyToFilePath(data.NameAndType(), true);
    if (!file_path) {
      return ConvertError<void>(file_path);
    }

    if (!boost::filesystem::exists(*file_path)) {
      return ConvertToFuture(Write(*file_path, data.Value()));
    }
  }
  return boost::make_ready_future();
}

boost::future<ImmutableData> DiskBackend::DoGetChunk(const ImmutableData::NameAndTypeId& name) {
  boost::expected<std::vector<byte>, common_error> data{};
  {
    const std::lock_guard<std::mutex> lock{mutex_};
    const auto file_path = KeyToFilePath(name, false);
    if (!file_path) {
      return ConvertError<ImmutableData>(file_path);
    }

    data = ReadFile(*file_path);
    if (!data) {
      return ConvertError<ImmutableData>(data);
    }
  }
  return boost::make_ready_future(ImmutableData{NonEmptyString{std::move(*data)}});
}

bool DiskBackend::HasDiskSpace(std::uintmax_t required_space) const {
  assert(current_disk_usage_ <= max_disk_usage_);
  return (max_disk_usage_ - current_disk_usage_) >= required_space;
}

Expected<boost::filesystem::path> DiskBackend::KeyToFilePath(
    const Data::NameAndTypeId& key, bool create_if_missing) const {
  const auto file_name = ::maidsafe::detail::GetFileName(key).string();

  if (file_name.empty()) {
    return boost::make_unexpected(make_error_code(CommonErrors::outside_of_bounds));
  }

  boost::filesystem::path disk_path{disk_path_};
  auto character = file_name.begin();
  {
    const auto last_character = file_name.end() - 1;
    for (unsigned i = 0; i < kDepth && character != last_character; ++i, ++character) {
      disk_path.append(character, character + 1);
    }
  }

  if (create_if_missing) {
    boost::system::error_code error{};
    boost::filesystem::create_directories(disk_path, error);
    if (error) {
      return boost::make_unexpected(make_error_code(CommonErrors::filesystem_io_error));
    }
  }

  return disk_path.append(character, file_name.end());
}

Expected<std::unique_ptr<StructuredDataVersions>> DiskBackend::ReadVersions(
      const Data::NameAndTypeId& key) const {
  return KeyToFilePath(key, false).bind(
      [] (boost::filesystem::path file_path) -> Expected<std::unique_ptr<StructuredDataVersions>> {
        file_path += ".ver";
        if (boost::filesystem::exists(file_path)) {
          auto file_data = ReadFile(file_path);
          if (!file_data) {
            return boost::make_unexpected(std::error_code(file_data.error().code()));
          }
          return maidsafe::make_unique<StructuredDataVersions>(
              StructuredDataVersions::serialised_type{NonEmptyString{std::move(*file_data)}});
        }

        return boost::make_unexpected(make_error_code(VaultErrors::no_such_account));
      });
}

Expected<void> DiskBackend::WriteVersions(
    const Data::NameAndTypeId& key, const StructuredDataVersions& versions, const bool creation) {
  if (!boost::filesystem::exists(disk_path_)) {
    return boost::make_unexpected(make_error_code(CommonErrors::filesystem_io_error));
  }

  return KeyToFilePath(key, true).bind(
      [&] (boost::filesystem::path file_path) -> Expected<void> {
        file_path += ".ver";
        if (boost::filesystem::exists(file_path)) {
          if (creation) {
            return boost::make_unexpected(make_error_code(VaultErrors::data_already_exists));
          }
          current_disk_usage_.data -= boost::filesystem::file_size(file_path);
        }
        return Write(file_path, versions.Serialise());
      });
}

Expected<void> DiskBackend::Write(
    const boost::filesystem::path& path, const NonEmptyString& value) {
  const auto value_size = value.string().size();

  if (!HasDiskSpace(value_size)) {
    LOG(kError) << "Out of space";
    return boost::make_unexpected(make_error_code(CommonErrors::cannot_exceed_limit));
  }
  if (!WriteFile(path, value.string())) {
    LOG(kError) << "Write failed";
    return boost::make_unexpected(make_error_code(CommonErrors::filesystem_io_error));
  }

  current_disk_usage_.data += value_size;
  return Expected<void>{boost::expect};
}

}  // namespace detail
}  // namespace nfs
}  // namespace maidsafe
