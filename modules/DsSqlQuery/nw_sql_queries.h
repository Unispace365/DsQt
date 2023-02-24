#ifndef NW_SQL_QUERIES_H
#define NW_SQL_QUERIES_H

#include <QMap>
QString resources = R"query(
SELECT r.*, m.type FROM resources r INNER JOIN media m ON r.media_id = m.id
)query";

QString qwaffle_nodes = R"query(
SELECT wn.*, ck.app_key AS kind, ck.app_key AS type, ck.branch FROM wfl_nodes wn
INNER JOIN cmsms_kinds_node ck ON wn.kind_id = ck.id
WHERE wn.enabled=1 AND (wn.deleted_at IS NULL OR wn.deleted_at = '') ORDER BY wn.rank DESC
)query";

QString qwaffles_text_fields = R"query(
SELECT wf.id, wf.node_id, cf.app_key, cf.label, wt.text_value as value, wt.plain_value
FROM wfl_fields wf
INNER JOIN cmsms_fields cf ON cf.id = wf.field_id
INNER JOIN wfl_content_texts wt ON wt.content_field_id = wf.id
WHERE cf.widget_type = 'text-widget'
AND cf.enabled = 1
)query";

QString qwaffles_checkboxes = R"query(
SELECT wf.id, wf.node_id, cf.app_key, cf.label, wt.checked as value
FROM wfl_fields wf
INNER JOIN cmsms_fields cf ON cf.id = wf.field_id
INNER JOIN wfl_content_checkboxes wt ON wt.content_field_id = wf.id
WHERE cf.widget_type = 'checkbox-widget'
AND cf.enabled = 1
)query";

QString qwaffles_color = R"query(
SELECT wf.id, wf.node_id, cf.app_key, cf.label, wt.hex as value
FROM wfl_fields wf
INNER JOIN cmsms_fields cf ON cf.id = wf.field_id
INNER JOIN wfl_content_color wt ON wt.content_field_id = wf.id
WHERE cf.widget_type = 'color-widget'
AND cf.enabled = 1
)query";

QString qwaffles_media_fields = R"query(
SELECT wf.id, wf.node_id, cf.enabled, cf.app_key, cf.label, wms.media_res, wms.media_thumb_res, wms.float_touch, wms.float_fullscreen, wms.float_autoplay, wms.float_loop, wms.float_volume, wms.float_page, cm.floatable, cm.hide_thumbnail, cf.widget_type
FROM wfl_fields wf
INNER JOIN cmsms_fields cf ON cf.id = wf.field_id
INNER JOIN cmsms_widget_media cm ON cm.id = cf.widget_id
INNER JOIN wfl_content_media wms ON wms.content_field_id = wf.id
WHERE cf.widget_type = 'media-widget' AND cf.enabled = 1
)query";

QString qwaffles_dropdown_fields = R"query(
SELECT wf.id, wf.node_id, cf.app_key, cf.label as dropdown_label, opts.label as option_label, opts.save_value as option_save_value, cds.allow_multiple
FROM wfl_fields wf
INNER JOIN cmsms_fields cf ON cf.id = wf.field_id
INNER JOIN cmsms_widget_dropdowns cds ON cds.id = cf.widget_id
INNER JOIN cmsms_opts_dropdowns opts ON opts.widget_id = cds.id
INNER JOIN wfl_content_dropdowns cont ON cont.option_id = opts.id AND cont.content_field_id = wf.id
WHERE widget_type = 'dropdown-widget'
AND enabled = 1
)query";

QString qwaffles_selections = R"query(
SELECT wf.id, wf.node_id AS owner_id, cf.app_key, wms.node_id, cm.allow_multiple, cf.id AS definition_id
            FROM wfl_fields wf
            INNER JOIN cmsms_fields cf ON cf.id = wf.field_id
            INNER JOIN cmsms_widget_selections cm ON cm.id = cf.widget_id
            INNER JOIN wfl_content_selections wms ON wms.content_field_id = wf.id
            WHERE cf.enabled=1 AND cf.widget_type = 'selection-widget'
            ORDER BY cf.rank ASC
)query";


QString qwaffles_hotspots = R"query(
SELECT wf.id, wf.node_id, cm.shape, cf.app_key, wms.pos_x, wms.pos_y, wms.pos_h, wms.pos_w
            FROM wfl_fields wf
            INNER JOIN cmsms_fields cf ON cf.id = wf.field_id
            INNER JOIN cmsms_widget_hotspots cm ON cm.id = cf.widget_id
            INNER JOIN wfl_content_hotspots wms ON wms.content_field_id = wf.id
            WHERE cf.enabled=1
            ORDER BY cf.rank ASC
)query";

QString qwaffles_composites = R"query(
SELECT wf.id, wf.node_id, cf.app_key, wms.pos_x, wms.pos_y, wms.pos_w
            FROM wfl_fields wf
            INNER JOIN cmsms_fields cf ON cf.id = wf.field_id
            INNER JOIN cmsms_widget_composites cm ON cm.id = cf.widget_id
            INNER JOIN wfl_content_composites wms ON wms.content_field_id = wf.id
            WHERE cf.enabled=1 AND cf.widget_type='composite-position-widget'
            ORDER BY cf.rank ASC
)query";

QString qwaffles_composite_details = R"query(
SELECT wf.id, wf.node_id, cf.app_key, wms.preview_res, wms.preview_thumb_res
            FROM wfl_fields wf
            INNER JOIN cmsms_fields cf ON cf.id = wf.field_id
            INNER JOIN cmsms_widget_composites cm ON cm.id = cf.widget_id
            INNER JOIN wfl_content_composite_details wms ON wms.content_field_id = wf.id
            WHERE cf.enabled=1 AND cf.widget_type='composite-widget'
            ORDER BY cf.rank ASC
)query";

QString qwaffles_tags = R"query(
SELECT t.id, tbs.taggable_field, t.title, t.class, wfl.id as node_id
            FROM taggables tbs
            INNER JOIN tags t on t.id = tbs.tag_id
            INNER JOIN wfl_nodes wfl on tbs.taggable_id = wfl.id
)query";

QString qwaffles_streamconfs = R"query(
SELECT wf.id, wf.node_id, cf.app_key, wms.location, wms.stream_res, r.resourcesfilename, r.media_title
            FROM wfl_fields wf
            INNER JOIN cmsms_fields cf ON cf.id = wf.field_id
            INNER JOIN cmsms_widget_streamconfs cm ON cm.id = cf.widget_id
            INNER JOIN wfl_content_streamconfs wms ON wms.content_field_id = wf.id
            INNER JOIN resources r ON wms.stream_res = r.resourcesid
            WHERE cf.enabled=1 AND cf.widget_type = 'streamconf-widget'
            ORDER BY cf.rank ASC
)query";


#endif // NW_SQL_QUERIES_H
