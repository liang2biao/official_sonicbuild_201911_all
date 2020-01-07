#include "../inc/sai_adapter.h"
#include <net/if.h>

sai_status_t sai_adapter::create_switch(sai_object_id_t *switch_id,
                                        uint32_t attr_count,
                                        const sai_attribute_t *attr_list) {
  (*logger)->info("create switch");
  // bool deafult_mac_set = false;
  // bool fdb_notification_set = false;
  // bool port_notification_set = false;
  sai_status_t status;
  for (int j=0; j<attr_count; j++) {
    (*logger)->info("attr id = {}", attr_list[j].id);
    switch (attr_list[j].id) {
      case SAI_SWITCH_ATTR_INIT_SWITCH:
        if (attr_list[j].value.booldata) {
          // status = init_switch(deafult_mac_set, fdb_notification_set, port_notification_set);
          status = init_switch();
          if (status != SAI_STATUS_SUCCESS) {
            return status;
          }
        }
        break;
      // case SAI_SWITCH_ATTR_SRC_MAC_ADDRESS:
        // deafult_mac_set = true;
        // set_switch_attribute(switch_metadata_ptr->switch_id, &attr_list[j]);
        // break;
      // case SAI_SWITCH_ATTR_FDB_EVENT_NOTIFY:
      //   set_switch_attribute(switch_metadata_ptr->switch_id, &attr_list[j]);
      //   break;
      // case SAI_SWITCH_ATTR_PORT_STATE_CHANGE_NOTIFY:
      //   set_switch_attribute(switch_metadata_ptr->switch_id, &attr_list[j]);
      //   break;
      default:
        set_switch_attribute(switch_metadata_ptr->switch_id, &attr_list[j]);
        break;
    }
  }
  *switch_id = switch_metadata_ptr->switch_id;
  return SAI_STATUS_SUCCESS;
}

int sai_adapter::get_port_ifi_index(int hw_port) {
  int ns_fd = open("/var/run/netns/sw_net", O_RDONLY);
  if (ns_fd == -1) {
    (*logger)->error("error opening sw_net fd");
  }
  int orig_ns_fd = open("/proc/self/ns/net", O_RDONLY);
  if (orig_ns_fd == -1) {
    (*logger)->error("error opening original fd");
  }
  if (setns(ns_fd, 0) == -1) {
    (*logger)->error("failed setting namespace sw_net");
    return -1;
  }
  std::string if_name = "sw_port" + std::to_string(hw_port);
  int ifi_index = if_nametoindex(if_name.c_str());
  (*logger)->info("if {} index {}", if_name, ifi_index);
  if (setns(orig_ns_fd, 0) == -1) {
    (*logger)->error("failed setting namespace sw_net");
    return -1;
  }
  return ifi_index;
}

sai_status_t sai_adapter::init_switch() {
  if (switch_list_ptr->size() != 0) {
    (*logger)->info(
        "currently one switch is supportred, returning operating switch_id: {}",
        (*switch_list_ptr)[0]);
    return SAI_STATUS_ITEM_ALREADY_EXISTS;
  } else {
    BmAddEntryOptions options;
    BmMatchParams match_params;
    BmMtEntry entry;
    BmActionData action_data;

    // Create Default 1Q Bridge, Vlan and switch_obj (not sure if switch is
    // needed).
    Bridge_obj *bridge = new Bridge_obj(sai_id_map_ptr);
    (*logger)->info("Default 1Q bridge. sai_object_id {} bridge_id {}",
                    bridge->sai_object_id, bridge->bridge_id);
    Sai_obj *switch_obj = new Sai_obj(sai_id_map_ptr);
    switch_metadata_ptr->switch_id = switch_obj->sai_object_id;
    switch_metadata_ptr->bridges[bridge->sai_object_id] = bridge;
    switch_metadata_ptr->default_bridge_id = bridge->sai_object_id;

    Vlan_obj *vlan = new Vlan_obj(sai_id_map_ptr);
    switch_metadata_ptr->vlans[vlan->sai_object_id] = vlan;
    vlan->vid = 1;
    vlan->bridge_id = switch_metadata_ptr->GetNewBridgeID(vlan->vid);
    vlan->handle_mc_mgrp = 0;
    vlan->handle_mc_l1 = 0;
    match_params.push_back(parse_exact_match_param(vlan->bridge_id, 2));
    switch_metadata_ptr->default_vlan_oid = vlan->sai_object_id;
    (*logger)->info("Default vlan id 1 (oid = {})", vlan->sai_object_id);
    for (std::vector<int>::iterator it = switch_metadata_ptr->hw_port_list.begin(); it != switch_metadata_ptr->hw_port_list.end(); ++it) {
      int hw_port = *it;

      // Create Default Ports (one for each hw_port)
      Port_obj *port = new Port_obj(sai_id_map_ptr);
      switch_metadata_ptr->ports[port->sai_object_id] = port;
      port->hw_port = hw_port;
      port->l2_if = hw_port;
      port->ifi_index = get_port_ifi_index(hw_port);
      (*logger)->info("Default port_id {}. hw_port = {}", port->sai_object_id,
                      port->hw_port);

      // Create Default Bridge ports and connect to 1Q bridge
      BridgePort_obj *bridge_port = new BridgePort_obj(sai_id_map_ptr);
      switch_metadata_ptr->bridge_ports[bridge_port->sai_object_id] =
          bridge_port;
      bridge_port->port_id = port->sai_object_id;
      bridge_port->bridge_port = hw_port;
      bridge_port->bridge_id = bridge->sai_object_id;
      bridge->bridge_port_list.push_back(bridge_port->sai_object_id);
      (*logger)->info("Default bridge_port_id {}. bridge_port = {}",
                      bridge_port->sai_object_id, bridge_port->bridge_port);

      // add default bridge ports to default vlan
      Vlan_member_obj *vlan_member = new Vlan_member_obj(sai_id_map_ptr);
      switch_metadata_ptr->vlan_members[vlan_member->sai_object_id] =
          vlan_member;
      vlan_member->vlan_oid = switch_metadata_ptr->default_vlan_oid;
      vlan_member->vid = vlan->vid;
      vlan_member->bridge_port_id = bridge_port->sai_object_id;
      vlan_member->tagging_mode = SAI_VLAN_TAGGING_MODE_UNTAGGED;
      vlan->vlan_members.push_back(vlan_member->sai_object_id);

      // Store default table entries
      match_params.clear();
      match_params.push_back(parse_exact_match_param(port->l2_if, 1));
      bm_bridge_client_ptr->bm_mt_get_entry_from_key(
          entry, cxt_id, "table_port_ingress_interface_type", match_params,
          options);
      bridge_port->handle_port_ingress_interface_type = entry.entry_handle;
      match_params.clear();
      match_params.push_back(
          parse_exact_match_param(bridge_port->bridge_port, 1));
      bm_bridge_client_ptr->bm_mt_get_entry_from_key(
          entry, cxt_id, "table_egress_br_port_to_if", match_params, options);
      bridge_port->handle_egress_br_port_to_if = entry.entry_handle;

      match_params.clear();
      match_params.push_back(parse_exact_match_param(port->l2_if, 1));
      match_params.push_back(parse_exact_match_param(vlan_member->vid, 2));
      match_params.push_back(parse_valid_match_param(true));
      bm_bridge_client_ptr->bm_mt_get_entry_from_key(
          entry, cxt_id, "table_egress_vlan_tag", match_params, options);
      vlan_member->handle_egress_vlan_tag = entry.entry_handle;
      match_params.clear();
      match_params.push_back(
          parse_exact_match_param(bridge_port->bridge_port, 1));
      match_params.push_back(parse_exact_match_param(vlan_member->vid, 2));
      bm_bridge_client_ptr->bm_mt_get_entry_from_key(
          entry, cxt_id, "table_egress_vlan_filtering", match_params, options);
      vlan_member->handle_egress_vlan_filtering = entry.entry_handle;
      bm_bridge_client_ptr->bm_mt_get_entry_from_key(
          entry, cxt_id, "table_ingress_vlan_filtering", match_params, options);
      vlan_member->handle_ingress_vlan_filtering = entry.entry_handle;
    }

    // Create CPU Ports
    Port_obj *port = new Port_obj(sai_id_map_ptr);
    switch_metadata_ptr->ports[port->sai_object_id] = port;
    port->hw_port = 250;
    port->l2_if = 250;
    port->internal = true;
    (*logger)->info("CPU port_id {}. hw_port = {}", port->sai_object_id,
                    port->hw_port);
    switch_metadata_ptr->cpu_port_id = port->sai_object_id;

    // Create Bridge Router port and bridge_port
    port = new Port_obj(sai_id_map_ptr);
    switch_metadata_ptr->ports[port->sai_object_id] = port;
    port->hw_port = 251;
    port->l2_if = 251;
    port->internal = true;
    (*logger)->info("Router port_id {}. hw_port = {}", port->sai_object_id,
                    port->hw_port);
    BridgePort_obj *bridge_port = new BridgePort_obj(sai_id_map_ptr);
    switch_metadata_ptr->bridge_ports[bridge_port->sai_object_id] = bridge_port;
    switch_metadata_ptr->router_bridge_port = bridge_port;
    bridge_port->bridge_port_type = SAI_BRIDGE_PORT_TYPE_1Q_ROUTER;
    bridge_port->port_id = port->sai_object_id;
    bridge_port->bridge_port = port->hw_port;
    bridge_port->bridge_id = bridge->sai_object_id;
    bridge->bridge_port_list.push_back(bridge_port->sai_object_id);
    (*logger)->info("Router bridge_port_id {}. bridge_port = {}",
                    bridge_port->sai_object_id, bridge_port->bridge_port);

    match_params.clear();
    match_params.push_back(
        parse_exact_match_param(bridge_port->bridge_port, 1));

    bm_bridge_client_ptr->bm_mt_get_entry_from_key(
        entry, cxt_id, "table_egress_br_port_to_if", match_params, options);
    bridge_port->handle_egress_br_port_to_if = entry.entry_handle;

    // Default virtual router and default vlan rif
    VirtualRouter_obj *vr = new VirtualRouter_obj(sai_id_map_ptr);
    switch_metadata_ptr->vrs[vr->sai_object_id] = vr;
    switch_metadata_ptr->default_vr_id = vr->sai_object_id;

    RouterInterface_obj *rif = new RouterInterface_obj(sai_id_map_ptr);
    switch_metadata_ptr->rifs[rif->sai_object_id] = rif;

    // Default Trap Group
    HostIF_Trap_Group_obj *hostif_trap_group = new HostIF_Trap_Group_obj(sai_id_map_ptr);
    switch_metadata_ptr->hostif_trap_groups[hostif_trap_group->sai_object_id] = hostif_trap_group;
    switch_metadata_ptr->default_trap_group = hostif_trap_group->sai_object_id;

    match_params.clear();
    match_params.push_back(parse_exact_match_param(0, 2)); //default port 0
    match_params.push_back(parse_exact_match_param(1, 2)); //dfeault vid 1
    bm_router_client_ptr->bm_mt_get_entry_from_key(
        entry, cxt_id, "table_ingress_l3_if", match_params, options);
    rif->handle_ingress_l3 = entry.entry_handle;

    match_params.clear();
    match_params.push_back(parse_exact_match_param(rif->rif_id, 1));
    bm_router_client_ptr->bm_mt_get_entry_from_key(
        entry, cxt_id, "table_egress_L3_if", match_params, options);
    rif->handle_egress_l3 = entry.entry_handle;

    bm_router_client_ptr->bm_mt_get_entry_from_key(
        entry, cxt_id, "table_ingress_vrf", match_params, options);
    rif->handle_ingress_vrf = entry.entry_handle;

    // Create MAC learn hostif_table_entry
    HostIF_Table_Entry_obj *hostif_table_entry =
        new HostIF_Table_Entry_obj(sai_id_map_ptr);
    switch_metadata_ptr
        ->hostif_table_entries[hostif_table_entry->sai_object_id] =
        hostif_table_entry;
    hostif_table_entry->entry_type = SAI_HOSTIF_TABLE_ENTRY_TYPE_TRAP_ID;
    hostif_table_entry->trap_id = MAC_LEARN_TRAP_ID;
    add_hostif_trap_id_table_entry(MAC_LEARN_TRAP_ID, learn_mac);
    switch_list_ptr->push_back(switch_obj->sai_object_id);
    return SAI_STATUS_SUCCESS;
  }
}

sai_status_t sai_adapter::remove_switch(sai_object_id_t switch_id) {
  return SAI_STATUS_NOT_IMPLEMENTED;
}

sai_status_t sai_adapter::get_switch_attribute(sai_object_id_t switch_id,
                                               sai_uint32_t attr_count,
                                               sai_attribute_t *attr_list) {
  (*logger)->info("get_switch_attribute");
  int i;
  int index; 
  int non_int_ports_num;
  for (i = 0; i < attr_count; i++) {
    switch (attr_list[i].id) {
      case SAI_SWITCH_ATTR_DEFAULT_1Q_BRIDGE_ID:
        attr_list[i].value.oid = switch_metadata_ptr->default_bridge_id;
        break;
      case SAI_SWITCH_ATTR_PORT_LIST:
        non_int_ports_num = std::count_if(
          switch_metadata_ptr->ports.begin(), 
          switch_metadata_ptr->ports.end(), 
          [](std::pair<sai_object_id_t, Port_obj*> p){return !(p.second->internal);}
        );
        if (attr_list[i].value.objlist.count < non_int_ports_num) {
          (*logger)->error("Buffer size ({}) is too small for number of ports in switch {}", attr_list[i].value.objlist.count, non_int_ports_num);
          return SAI_STATUS_BUFFER_OVERFLOW;
        }
        index = 0;
        for (port_id_map_t::iterator it = switch_metadata_ptr->ports.begin();
             it != switch_metadata_ptr->ports.end(); ++it) {
          if (!it->second->internal) {
            attr_list[i].value.objlist.list[index] = it->first;
            index += 1;
          }
        }
        attr_list[i].value.objlist.count = index;
        break;
      case SAI_SWITCH_ATTR_PORT_NUMBER:
        attr_list[i].value.u32 = std::count_if(
          switch_metadata_ptr->ports.begin(), 
          switch_metadata_ptr->ports.end(), 
          [](std::pair<sai_object_id_t, Port_obj*> p){return !(p.second->internal);}
        );
        break;
      case SAI_SWITCH_ATTR_DEFAULT_VLAN_ID:
        attr_list[i].value.oid = switch_metadata_ptr->default_vlan_oid;
        break;
      case SAI_SWITCH_ATTR_CPU_PORT:
        attr_list[i].value.oid = switch_metadata_ptr->cpu_port_id;
        break;
      case SAI_SWITCH_ATTR_DEFAULT_VIRTUAL_ROUTER_ID:
        attr_list[i].value.oid = switch_metadata_ptr->default_vr_id;
        break;
      case SAI_SWITCH_ATTR_SRC_MAC_ADDRESS:
        memcpy(attr_list[i].value.mac, switch_metadata_ptr->default_switch_mac, 6);
        break;
      case SAI_SWITCH_ATTR_DEFAULT_TRAP_GROUP:
        attr_list[i].value.oid = switch_metadata_ptr->default_trap_group;
        break;
      case SAI_SWITCH_ATTR_NUMBER_OF_ECMP_GROUPS:
        attr_list[i].value.u32 = 0;
        break;
      case SAI_SWITCH_ATTR_SWITCH_HARDWARE_INFO:
        attr_list[i].value.s8list.list[0] = '\0';
        attr_list[i].value.s8list.count = 1;
        break;
      case SAI_SWITCH_ATTR_ACL_TABLE_MINIMUM_PRIORITY:
      case SAI_SWITCH_ATTR_ACL_ENTRY_MINIMUM_PRIORITY:
      case SAI_SWITCH_ATTR_ACL_TABLE_GROUP_MINIMUM_PRIORITY:
        attr_list[i].value.u32 = 10;
        break;
      case SAI_SWITCH_ATTR_ACL_ENTRY_MAXIMUM_PRIORITY:
      case SAI_SWITCH_ATTR_ACL_TABLE_GROUP_MAXIMUM_PRIORITY:
      case SAI_SWITCH_ATTR_ACL_TABLE_MAXIMUM_PRIORITY:
      attr_list[i].value.u32 = 1000;
        break;
      default:
        (*logger)->error("unsupported switch attribute {}", attr_list[i].id);
        return SAI_STATUS_NOT_IMPLEMENTED;
    }
  }
  return SAI_STATUS_SUCCESS;
}

sai_status_t sai_adapter::set_switch_attribute(sai_object_id_t switch_id,
                                               const sai_attribute_t *attr) {
  (*logger)->info("set_switch_attribute ({})", attr->id);
  switch (attr->id) {
    case SAI_SWITCH_ATTR_SRC_MAC_ADDRESS:
      memcpy(switch_metadata_ptr->default_switch_mac, attr->value.mac, 6);
      (*logger)->info("default switch mac set to:");
      print_mac_to_log(switch_metadata_ptr->default_switch_mac, *logger);
      break;
    case SAI_SWITCH_ATTR_FDB_EVENT_NOTIFY:
      (*logger)->info("fdb event notification funciton was set");
      switch_metadata_ptr->fdb_event_notification_fn = (sai_fdb_event_notification_fn) attr->value.ptr;
      break;
    case SAI_SWITCH_ATTR_PORT_STATE_CHANGE_NOTIFY:
      (*logger)->info("port state notification funciton was set");
      switch_metadata_ptr->port_state_change_notification_fn = (sai_port_state_change_notification_fn) attr->value.ptr;
      break;
    default:
      (*logger)->info("unsupported switch attribute {}", attr->id);
      break;
  }
  return SAI_STATUS_SUCCESS;
}
